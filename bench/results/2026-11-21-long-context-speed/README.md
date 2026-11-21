# Why speed falls with the context window, and what keeps it (21 Nov 2026)

Autoresearch loop on 2x RX 7900 XTX (gfx1100, PCIe 3.0 x8), ROCm 7.9, Swift 1.5 IQ3_S, `--max-context 262144`,
`--kv int8 --kv-resident 32768`, the production config of 21 Nov (`strata-swift-iq3_s-tuned.json`). About 40
fresh-server runs; every row below is a fresh engine start.

## Result

| | before | after | |
|---|---|---|---|
| prompt read, 128K tokens | 3,706-3,735 tok/s | 4,054-4,076 tok/s | +10% |
| prompt read, 240K tokens | 3,390-3,420 tok/s | 4,063-4,106 tok/s | +20% |
| prompt read, 8K / 4K / 16K / 32K | 2,014 / 1,440 / 2,690 / 3,280 | the same | |
| decode window at 8K | 17.0-17.5 ms | 17.0-17.6 ms | |
| decode window at 128K | 17.9-18.3 ms | 17.7-17.9 ms | -2% |
| decode window at 240K | 18.8-19.2 ms | 18.2-18.3 ms | -4% |
| short decode, 8 prompts (median) | 114-117 tok/s | 113-115 tok/s | within start-to-start noise |

A 240K prompt now reads as fast as a 128K one (59 s instead of 70 s). Decode at 240K is about 5% slower per window
than at 8K, where it was about 10%.

In production since 21 Nov: `engine/strata-prod-1121b` with four more environment switches,

```
STRATA_SCORES_THREAD=1   STRATA_KV_PREFETCH=1   STRATA_QSA_ROWS=1   STRATA_SELECT_LONG=1
```

Rollback: `exe` back to `engine/strata-prod-1121` and drop the four keys (`STRATA_SELECT_LONG` is ignored by the old
engine with a note; the other three exist in it).

## The cause

Attention in this model is sparse (QSA): a layer scores one pooled key per 4 context tokens for every query, picks the
best 2,048 cells and attends over those. Twelve of the 48 layers do this. The attention itself costs the same at any
depth; the scoring and the pick read the whole context.

**Prompt reading** (`STRATA_PREFILL_TIMING=1`, one card, a 12,032-token chunk):

| chunk position | chunk | selection | K/V staged from RAM | everything else |
|---|---|---|---|---|
| start | 2,376 ms | 86 ms | 0 ms | 2,290 ms |
| 66K deep | 2,805 ms | 422 ms | 82 ms | 2,300 ms |
| 132K deep | 3,254 ms | 772 ms | 164 ms | 2,320 ms |
| 230K deep | 3,978 ms | 1,360 ms | 287 ms | 2,330 ms |

- Selection: each of the chunk's 12K tokens against every pooled key before it, so the work per chunk grows with the
  depth. Over a 240K prompt it is 14.0 s of 63 s on one card.
- K/V staging: with `--kv-resident`, each attention layer's earlier K/V is copied from pinned RAM to the card again for
  every chunk (PCIe-bound, about 5 GB/s). 2.4 s of the 240K prompt.

**Decode** (`STRATA_VERIFY_PROFILE=1`, ms per window, card 1 / card 2):

| context | scores + pick | K/V resolve | attention |
|---|---|---|---|
| 8K | 0.20 / 0.22 | 0.12 / 0.14 | 0.37 / 0.41 |
| 128K | 0.60 / 0.69 | 0.20 / 0.16 | 0.38 / 0.43 |
| 240K | 1.01 / 1.17 | 0.22 / 0.19 | 0.38 / 0.44 |

Only "scores + pick" grows: about 1.75 ms more per window at 240K, which is the measured rise of the whole window
(17.4 -> 19.0 ms). In isolation (`qsa_scores_gfx1100_parity --dec`, idle card, 2 queries, 240K): the scorer 0.12 ms and
the pick 0.10 ms per call. The scorer reduces each head with five wave shuffles per block; the pick runs in one thread
block per query. Tokens per second also move with the text (drafts accepted per window), which is why single
tok/s readings at different depths disagree by more than this.

## What was kept

| switch | what it does | measured | exact? |
|---|---|---|---|
| `STRATA_SCORES_THREAD=1` | upstream's four-lanes-per-query prompt scorer; on by default on gfx12 only | selection 14.0 -> 5.1 s over a 240K prompt; +6% at 128K, +15% at 240K | bitwise (`qsa_scores_gfx1100_parity --full`: 35 cases on this card) |
| `STRATA_KV_PREFETCH=1` | upstream's overlap of the next layer's K/V upload with the current layer's compute | +2% at 240K alone, +6% on top of the scorer | ordering only |
| `STRATA_QSA_ROWS=1` | upstream's decode scorer with the queries in registers (documented for CUDA; runs on HIP) | scores + pick at 240K 1.01 / 1.17 -> 0.79 / 0.92 ms; nothing at 8K | bitwise (its doc; here through `--dec`) |
| `STRATA_SELECT_LONG=1` | upstream's second set of decode graphs past 65,536 cells; **this branch gives it kernels on HIP** | scores + pick at 240K -> 0.57 / 0.63 ms, at 128K 0.60 / 0.69 -> 0.45 / 0.49 ms; 8K untouched | bitwise scores and ids (`--dec`: 82 cases) |

Code (this branch, `engine/src/kernels/cuda/qsa_select.cu`, HIP only; CUDA code paths are token-for-token unchanged):

- `block_scores_dec_kernel`: the decode scorer with a thread per key block. A thread keeps the block's 32 float4 of key
  in registers and adds a head's 32 partial products in the shuffle tree's pairing (l with l+16, then +8, +4, +2, +1),
  so every addition has the multi kernel's operands. 2.4-3.1x faster than the multi kernel at 240K in isolation, slower
  below ~30K (one thread does 512 products per query), which is why it sits behind the long graphs only.
  `STRATA_SCORES_DEC=0` turns it off.
- The split pick (`topk_split_*`, written upstream for Turing, compiled out on HIP): compiled for HIP with its
  histograms, states and counts in the caller's scratch instead of device globals, one zeroing kernel instead of a
  memset. 0.10 -> 0.045 ms a call at 240K; 0.06 against 0.04 ms at 8K, so again only behind the long graphs.
  `STRATA_TOPK_SPLIT=0` turns it off.
- `qsa_topk_multi_scratch_ints`, `qsa_block_scores_long`, `qsa_block_topk_long` answer on HIP, so the verifier's
  `STRATA_SELECT_LONG=1` (verify.cpp, unchanged) allocates the scratch and keeps the second graphs.
- `qsa_scores_gfx1100_parity --dec`: 82 cases through the long entry points (1-8 queries, contexts 9 to 262,140,
  zero / NaN / infinity / tie inputs) against the multi kernel's scores and, with `STRATA_TOPK_REF=1`, the original
  pick's ids; prints both scorers' and both picks' times.

Checks on the installed set: needles 9/9 at 30K / 128K / 236K and 6/6 short answers (`validate.py`, twice), smoke 7/7,
0 engine error lines, production restarted. Answers are not the same text from one engine start to the next even at
baseline (two baseline starts gave six different hashes for six answers), so "same text" could not be the exactness
check; the kernel-level bit comparisons are.

## What was tried and dropped

| try | result |
|---|---|
| `--kv-resident 0` (all K/V in VRAM, no staging) | prompts slower: 3,444 / 3,229 tok/s at 128K / 240K; it takes 1,470 expert slots |
| `STRATA_SELECT_SIMT=1` / `=2` (FP32 tiled prompt scorers, another summation order) | 4,090 / 4,135 tok/s at 240K: the exact scorer with the prefetch is as fast (4,188) |
| `STRATA_TOPK_STREAM=0` on the prompt path | selection 5.1 -> 6.2 s |
| `STRATA_SCORES_HALVE=1`, `STRATA_SCORES_MULTI=0`, `STRATA_TOPK_OLD=1` on decode | 1.02 / 1.19, 0.87 / 0.99 and 1.78 / 2.07 ms at 240K |
| the thread-per-block decode scorer on every window (not only the long graphs) | 240K 0.83 / 0.95 ms, but 8K 0.25 / 0.28 (from 0.20 / 0.22) |
| a thread per (block, query, head) scorer | bitwise, but 0.6x the multi kernel at 2-4 queries |
| the 66-key register pick for decode at a 262K capacity | 8K pick 0.04 -> 0.03 ms in isolation, 240K worse (0.13); in the engine 1.06 / 1.16 ms at 240K |
| the split pick outside the long graphs with its own allocation | the capture of a window graph fails (an allocation inside the capture) |
| `--max-context 131072` | scores + pick 0.15 / 0.17 ms at 8K (register pick) and 0.55 / 0.60 at 120K: small |

## What is left

- Decode at 240K still pays about 0.8 ms per window over 8K: seven launches per pick and one per scorer in each of
  twelve layers, and the K/V resolve (+0.1 ms). A scorer and pick fused into fewer launches is the next step there.
- Prompt reading at depth: selection is still 380 ms of the deepest chunk (86 at the start), and the K/V staging
  moved into "wait copy" rather than away. Keeping the staged prefix between chunks (uploading only the new chunk's
  cells) would remove it; it needs staging-pool space for the whole context on each card.
- Not measured: several long conversations at once, and sampled requests at depth.

## Files

- `scripts/`: `exp.sh LABEL EXE "ENV=.." "args" reps sizes` (fresh server on 8091 with the production config plus
  changes), `ctx.py` (per size one cold prompt, then two more answers on the cached prefix; prompt tok/s, decode tok/s,
  ms per window), `summ.py` (prompt phases at depth), `prof.py` (decode profile lines), `validate.py` (needles to 236K),
  the queue scripts `q1.sh` .. `q11.sh`.
- `data/`: the queues' outputs (`q*.txt`), `results.tsv`, and the engine's timing lines of four runs.
- Production measured afterwards with the 21 Nov script (`../2026-11-21-hotrod-comparison/scripts/longdec.py 8080`):
  127K prompt 4,106 tok/s (31 s), 242K prompt 4,117 tok/s (59 s); before: 3,814 (33 s) and 3,465 (70 s).
