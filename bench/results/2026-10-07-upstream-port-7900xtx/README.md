# Our 2x RX 7900 XTX changes on upstream's new main (2026-10-06/07)

Upstream's `main` was rewritten on 2026-10-06 with 1,159 commits our branches did not have. Branch
`amd-7900xtx-port` is upstream `82f46a8` plus our changes, ported one by one (cherry-picked, conflicts resolved by
hand, each built and checked). Same PC, model and config as bench/results/2026-10-05-split-decode-7900xtx (2x RX 7900
XTX on PCIe 3.0 x8, Ryzen 9 5950X, ROCm 7.9.0, IQ3_XXS, 128K context, int8 KV with 32K resident, MTP,
`--gpus 1,0 --vram-reserve-mib 3072`); that folder's `bench.py` and the multi-agent folder's `mbench.py`.

**Result:** one conversation decodes at **92.7 tok/s with upstream's `--pipeline-windows 2`** (73.9 without it), and
eight conversations at once at **185.8 tok/s** (`"parallel": 8`).

## What was ported

| change | how it fit upstream |
|---|---|
| a split's commits do not wait | upstream added a one-token self-commit; the asynchronous branch kept inside it |
| a later stage's window launched early | upstream added an all-resident path (`refresh_ar`, two graphs per size, a plan-error word): `early_post` runs `refresh_ar` and clears the word on the main thread, the worker launches the graph it chose |
| the pipelined windows raise the later stage's `go` | new: upstream's `pl_launch` (the `--pipeline-windows` path) never raised the word the early launch adds to every later stage's graph; without it a pipelined window timed out at layer 26 |
| the CPU rows, the GPU rows and the combine in one kernel | upstream has its own all-resident version (`..._multi_hits`) and spells the combine as explicit `fmaf` (a float4 kernel for 10 experts): ours now uses `fmaf` too |
| batch groups without pad rows, the server's groups and slot choice, setup's note, the review fixes | upstream renamed `batch_key` to `bkey`; tests merged with upstream's new `--batch-mtp` test |

Upstream already had its own fix for the ROCm 7.9 `alignas` build error.

## One conversation (decode, greedy, story + code prompts x3, median)

| | tok/s | ms per window |
|---|---|---|
| upstream `main` as it was | 58.0 | 36.27 |
| + async commit | 60.0 | 35.15 |
| + early launch | 77.5 | 27.29 |
| + early launch, `--pipeline-windows 2` | 85.3-92.7 | 22.84-23.09 |
| `--pipeline-windows 2`, early launch off | 87.5 | 23.00 |
| (the old branch, for comparison) | 73-74 | 28.40 |

The upstream-only rows and the per-step port runs above (06 Oct) were measured before the machine was restarted; their
raw records were in a scratch folder that did not survive it. `data/results.jsonl` holds the runs of 07 Oct: the
serial and pipelined pair, the gate sweep, the stall tests, the multi-client runs and the stream retest.

With the pipeline the early launch adds nothing measurable (the cards already overlap); without it, it is +29%.
The merged combine kept the same tokens on the port and saved ~0.5% (31.21-31.24 vs 31.38 ms per window, fresh start,
`--adapt-every 0`): upstream's own combine work took part of what it saved before.

### How `--pipeline-windows 2` gets there (07 Oct, `R-pw2-full` vs `R-serial-full`)

Serial: 73.9 tok/s, 27.85 ms per window. Pipelined: 92.7 tok/s, 23.09 ms per window. The engine's own count for the
last request: 219 windows, 164 started on a guess, 70 of them right ("on the path", 14.58 ms and 2.51 tokens each),
94 rolled back, 54 not tried (the draft layer's estimate under the gate, theta 0.20); the 149 others ("fresh") took
26.51 ms and kept 2.26 tokens each - no slower than a serial window, since a wrong guess only used a card that
would otherwise have waited. The gate's estimate is well calibrated (its top decile: 25 of 27 kept).

| gate (`STRATA_PIPELINE_DEBUG=1 STRATA_PIPELINE_THETA`) | ms per window | guesses right / started | tok/s |
|---|---|---|---|
| 0.0 | 22.40 | 83 / 197 | 91.2 |
| 0.1 | 21.83 | 100 / 239 | 90.4 |
| 0.2 (default) | 23.09 | 70 / 164 | 92.7 |
| 0.3 | 24.11 | 59 / 127 | 87.0 |

A lower gate gives shorter windows, but the texts differ between runs and tok/s does not confirm it: a candidate
(~3-5% per window at 0.1) for more repeats, the default kept.

### Tuning the pipeline on this PC (07 Oct)

The gate and the draft settings change only when card 1 guesses, never what is emitted: with `STRATA_IQ_MT_MIN=1
--pcie-frac 0 --adapt-every 0` every run below wrote the same text (story 3e41c3, code 32e166), so tok/s compares
exactly (greedy story + code x3).

| setting (pipelined) | tok/s | |
|---|---|---|
| gate 0.2 (default), `--spec 4 --spec-min-p 0.5` | 86.4 (85.7, 87.0) / 88.0-88.5 | the reference |
| gate 0.1 | 89.0 (88.9, 89.0) | +3.0% |
| gate 0.0 | 89.4 (88.8, 89.9) | +3.5%, with 146 rollbacks against 117 |
| gate 0.1, `--spec 3` | 90.9 | +3.0% over gate 0.1 alone |
| gate 0.1, `--spec-min-p 0.7` | 91.7 (twice) | +3.9% |
| gate 0.1, `--spec 3 --spec-min-p 0.6` / `0.7` | 93.3 / 93.2 | +5.6% |
| gate 0.1, `--spec 5` / `--spec 6` / `--spec-min-p 0.3` | 85.5 / 81.7 / 81.5 | slower |
| gate 0.1, `--spec-min-p 0.8` / `0.9` / `--spec 3 --spec-min-p 0.8` | 88.9 / 90.3 / 89.8 | |

Why shorter drafting wins here, unlike the serial loop: `pltrace_sum.py` on a `STRATA_PIPELINE_TRACE` (the pipeline's
event log) shows each card busy only ~60% of the time, both halves equal (11.1 / 11.2 ms median), and the cycle after a
right guess 14.1 ms but after any other verdict 26.0 ms - the draft chain (4.0 ms, on card 2), then card 1's half, then
card 2's half, in turn. The chain sits on that path, so a shorter one pays even with fewer tokens per window.

In the installed config with the adaptive tier on (two interleaved pairs): `--spec 3 --spec-min-p 0.7` 105.7 / 92.9
tok/s against 93.1 / 89.7, windows 17.55-17.78 against 22.65-22.69 ms. The config now carries `--pipeline-windows 2
--spec 3 --spec-min-p 0.7` and `"env": {"STRATA_PIPELINE_DEBUG": "1", "STRATA_PIPELINE_THETA": "0.1"}` (the gate is
read only with the debug switch).

### The draft chain, profiled (07 Oct)

`chain_prof.py` on a `rocprofv3 --kernel-trace` of the pipelined decode (the drafter has its own stream on the last
card): 143 chains, 312 kernels and 3.92 ms of kernel time each (the 4.0 ms the pipeline trace measured without the
profiler).

| part of a chain | per chain | share |
|---|---|---|
| the draft head (`native_q5_k_mmvq`, 252 us x ~5: 106,299 tokens, 178 MiB, Q5_K) | 1.34 ms | 34% |
| the draft layer's dense projections (Q8_0) | ~0.55 ms | 14% |
| attention (`attn_merge` 60 us, `attn_chunk` 44 us, x5) | 0.56 ms | 14% |
| greedy sampling, top probability, selection | ~0.28 ms | 7% |
| the rest (hyper-connections, experts, norms, quantize) | ~1.2 ms | 31% |

- **The English + code draft subset** (`--draft-vocab en`: 40,525 tokens, 68 MiB) - drafts never change the output,
  so this is lossless; it only lowers acceptance for answers in scripts the subset lacks (CJK, Cyrillic). Same text,
  91.7 / 91.7 -> 94.4 / 94.5 tok/s (+3.0%), drafts accepted 77.5% -> 77.3%, and ~110 MiB of VRAM back to the expert
  cache. In the installed config (setup's way: `"draft_vocab": "en"`, data/draft_vocab_en.bin copied over the MTP
  folder's draft_vocab.bin): 96.8 tok/s.
- **The attention merge** walks every chunk of the capacity (the draft layer's window: 256 chunks of 64 cells) though
  the valid cells are a prefix of `n_ids`. Stopping its loops at `ceil(n_ids / 64)` kept the same text but measured
  no gain (94.7 / 93.1 vs 94.3 / 94.6 tok/s): not kept. (A first version also shortened the layout's stride between
  KV heads and changed the text - the exactness check caught it.)

## Several conversations at once (`"parallel": 8`, total tok/s, `mbench.py`)

| clients | the old branch | the port |
|---|---|---|
| 1 | 69 | 72.1 |
| 2 | 70 | 88.7 |
| 4 | 105 | 133.0 |
| 8 | 151 | 185.8 |

The last answer of eight started after 4.8 s. Upstream's `--batch-mtp` (MTP drafts in batch slots) is for one GPU
for now: on the split the engine says so and leaves it off (182.1 at 8, the same). `--pipeline-windows` is off with
batch slots by design.

## Tried and no change on the port

- `STRATA_SH_STREAM=0` (no shared-expert stream fork; 3-4% on the old branch): 31.75 vs 31.54 ms per window serial
  (the same tokens), 22.74 vs 23.02 pipelined - within noise.
- `--kv k8v4` (old branch, 06 Oct): it cannot stream the KV (`--kv-resident`); without streaming it was 29.11 vs int8's
  29.40 ms per window, both behind int8 with streaming (28.40, 83% vs 79-80% of the experts in VRAM) on short prompts.

## Checks

- ctest: 83 of 88. The four known failures (`ple_parity` needs the Q2_0 fixture, `expert_multi_test` an AVX-512 CPU,
  `platform_memory_test` the container's memlock limit, `hip_q2_zero` the gfx1012-only signed-zero case) and
  upstream's new `expert_cache_segmented_test`, which tests `--vram-elastic` - "CUDA-only for now" on a HIP build.
- `tools/batch_test.py` (`--layer-split 26 --batch-groups 2`): 3 of 8 and 8 of 8 slots IDENTICAL to their solo
  tokens; `tools/batch_interleave_test.py --batch 4`: A, B, C and A's next turn from its slot IDENTICAL ("slot 0 gave
  back 144 tokens"), its prompt-giving-way step fails as on the base engine.
- `serve/test_parallel.py`, `serve/test_server.py`, `tools/test_setup_parallel.py` pass.
- `smoke.py` with `--pipeline-windows 2`: 7 of 7, no error lines, decode 84-105 tok/s.
- `STRATA_TEST_VERIFY_STALL=30`: serial - the stall report, the release reached every wait (17 ms), both cards idle;
  pipelined - the hook is in the serial window path only, so it does not fire there (every request completed).
