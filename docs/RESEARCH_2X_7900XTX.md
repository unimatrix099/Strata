# Two RX 7900 XTX in one PC: what was measured and changed (2026-10-05)

One day of work on a PC with two AMD RX 7900 XTX: installing Strata, then making the two cards faster, first for one
conversation and then for several at once (coding agents and their sub-agents). Every number below was measured on
this PC; the details, raw results and tools are in the two result folders linked in each section.

| | before | after |
|---|---|---|
| one conversation, both cards (decode) | 55.0 tok/s (one card alone: 60) | **72.2 tok/s** |
| 4 conversations at once (in all) | 73.4 tok/s, the 4th answer starts after 21 s | **101-105 tok/s**, after 2.8-3.0 s |
| 8 conversations at once (in all) | 70.9 tok/s, the 8th answer starts after 51 s | **150-151 tok/s**, after 5.2-5.8 s |
| prompts (~4.6K tokens) | 1,245 tok/s | 1,241-1,246 tok/s (unchanged) |
| **on upstream's new main (07 Oct), one conversation, `--pipeline-windows 2`** | 58.0 tok/s (upstream as it was) | **92.7 tok/s** |
| **on upstream's new main, 8 conversations at once** | 70.9 tok/s | **185.8 tok/s** |
| **on upstream's new main, tuned (07 Oct), one conversation** | 92.7 tok/s | **96.8 tok/s** (story + code); 104-107 tok/s over eight prompts |

The answers are the same: greedy runs give the same tokens as before (checked by hashes and by the repository's
exactness tests).

## Summary: what is kept (07 Oct)

One conversation on both cards (IQ3_XXS, decode, tok/s):

| step | tok/s | gain |
|---|---|---|
| upstream's main as it was (06 Oct) | 58.0 | - |
| + the asynchronous commit (ours) | 60.0 | +3.4% |
| + the early launch of card 2's window (ours) | 77.5 | +29% |
| + upstream's `--pipeline-windows 2` (theirs, on in the config) | 92.7 | +20% |
| + gate 0.1, `--spec 3 --spec-min-p 0.7` (tuning) | | ~+8% |
| + the English + code draft vocabulary | **96.8** (installed config) | +3.0% |

- Overall 58.0 -> 96.8 tok/s, **+67%** over upstream as it was (the old code of 05 Oct: 55 -> 72, +31%; one card
  alone ~60). With the pipeline on, the early launch adds ~0 (the cards overlap already); without it, +29%.
- Small, kept: the merged combine (~+0.5%), `STRATA_QFUSE=1` (~+0.5%, inside the noise). Kept with no measurable
  gain in the installed config: the reserve per card (`--vram-reserve-mib 700 --vram-reserve-later-mib 3072`).

Several conversations at once (`"parallel"` slots, total tok/s): 4 at once 73.4 -> 133.0 (**+81%**), 8 at once
70.9 -> 185.8 (**+162%**); since the rebase (08 Oct) with upstream's own batching: 133.5 and 179.6.

Fixed: `STRATA_QFUSE=1` with `"parallel"` slots gave garbage text (upstream's fix #1139 since the rebase).

Tried, not kept: a later layer split, upstream's other gfx1151 switches, the resident head kernel,
`HIP_FORCE_DEV_KERNARG`, and the second branch for the bonus token (built lossless on branch `bonus-branch`, no gain).
Details in sections 6-8.

## The PC

- 2x AMD Radeon RX 7900 XTX (gfx1100, 24 GB each), both on PCIe 3.0 x8 (the engine's probe: 7.1 GB/s). HIP device 0
  (PCI 0a) also drives two monitors.
- Ryzen 9 5950X (16 cores, AVX2, no AVX-512), 126 GB RAM; Linux in a container without root; system ROCm 7.9.0
  (hipBLASLt 1.1.0).
- Model: Qwen3.8-Flash-Next IQ3_XXS (3-bit), 128K context, 8-bit KV with 32K resident, MTP drafts.

## 1. Installing (docs/AMD_HIP.md, "Two cards, one of them the desktop's")

- **Build on ROCm 7.9:** its clang rejects `__shared__ alignas(16)`; `iq_kernels.cu` uses `__shared__ __align__(16)`
  like the other kernels (commit `c6d4458`).
- **The container had no `python3-venv`:** the setup's `.venv` was made with `get-pip.py` (no root needed).
- **The desktop card:** with it as the first card and the default 700 MiB VRAM reserve, amdgpu moved 21 GB of the
  engine's memory to system RAM (GTT) while the engine's log said everything fit: 80 tok/s prompts, 15 tok/s decode.
  `--vram-reserve-mib 3072` fixed it (later measured: either card order then decodes the same). Only
  `/sys/class/drm/card*/device/mem_info_gtt_used` shows such a spill.

## 2. One conversation on both cards (bench/results/2026-10-05-split-decode-7900xtx)

A layer split runs layers 0-25 on one card and 26-47 on the other. A token needs layer 25's result before layer 26
can start, so the cards take turns; the work was to shorten the time between the turns.

- **Where the time went:** per decode window (~2 tokens, 39.6 ms) 16.1 + 14.0 ms of GPU work on the two cards, and
  the rest in the hand-off: stage 1's graph launch (1.25-1.5 ms of CPU for ~1,200 kernels on ROCm) only after stage 0
  had finished, synchronous commits, the draft. The split was slower than one card alone (54-55 vs 60 tok/s).
- **Kept:** the split's commits no longer wait (55.0 -> 57.5), and stage 1's window graph is launched early by a
  worker thread and waits on a flag until stage 0's result is there (57.5 -> 72.2). Part of the gain is clocks: RDNA3
  lowers the shader clock in idle gaps (1.5 GHz); the waiting card now spins and stays at ~2.95 GHz.
- **Dropped:** keeping stage 0's card busy with a spin kernel (no gain), HIP graph runtime switches, other speculation
  settings, another split point, the default VRAM reserve (the desktop card spilled again).
- **Safety:** an independent review found one bug in the early launch's error path (a failed window could leave the
  other card spinning); fixed and tested with the #267 stall hook.
- Switches: `STRATA_SPLIT_COMMIT_SYNC=1`, `STRATA_SPLIT_EARLY_LAUNCH=0`; one card never reaches this code.

## 3. Several conversations at once (bench/results/2026-10-05-multi-agent-7900xtx)

The engine already had batch slots (`--batch N`: one token of each conversation per window) and a pipeline for a
layer split (`--batch-groups G`: card A runs one group while card B runs another). On this PC it barely helped:

- **Pad rows:** a group always ran all its slots; an idle slot cost a dummy token's experts on every card. With 8
  slots and 2 requests: 39 tok/s in all, below one request at a time. Now a group runs only its active slots
  (70 tok/s), the tokens identical to each request alone.
- **No pipeline from the config:** `"parallel": N` gave only `--batch N` (all slots in one window, card after card).
  The server now adds `--batch-groups` on several GPUs (`"batch_groups": 1` turns it off): 97 -> 150 tok/s at 8.
- **Unlucky slot choice:** two requests in one group ran card after card (26 instead of 38 tok/s each); a new request
  now goes to the group with the fewest running ones.
- Pipelined slots keep their conversation cache again (an agent's next turn does not read its history again), and
  setup suggests `--parallel` on several cards where the slots fit.

Use: `"parallel": 8` in `strata-<model>.json` when two or more requests often run at once. Costs: a request alone is
~5% slower (the slots' VRAM leaves 75% instead of 83% of the experts in VRAM), and a request sharing the cards decodes
at 20-40 tok/s instead of ~78 (slots do not use MTP drafts).

## 4. Can the experts of one conversation run on both cards at the same time?

The model is a mixture of experts, so the question was whether one conversation's experts could be split across the
cards so that neither waits. What the profile says (`STRATA_VERIFY_PROFILE=1`, 4-row windows, per card and window):

| | stage 0 | stage 1 |
|---|---|---|
| GPU time per window | 18.4 ms | 17.9 ms |
| routed experts (VRAM hits, PCIe groups, copy + combine) | 3.9 ms | 3.4 ms |
| everything else (attention, recurrent layers, router, shared expert, head) | 14.5 ms | 14.5 ms |

- The layer split already spreads the experts: each card holds the experts of its own layers.
- Splitting each layer's experts between the cards (expert parallelism) could save at most half the routed experts'
  time, ~2 ms of an 18 ms window, and every layer would need two cross-card hand-offs (48 layers, each waiting on the
  other card); the rest of the layer - attention, the recurrent state, the router, the shared expert - cannot be split
  that way. Not worth it here; not built.
- Splitting every matrix across the cards (tensor parallelism) helped dense models a lot in llama.cpp on this same
  PC, but it would mean a new window graph in this engine and an all-reduce per layer over PCIe 3.0 x8. Not tried.
- **Measured on 2026-10-06** (bench/results/2026-10-06-expert-parallel-7900xtx): Strata's two expert-parallel modes
  run on AMD but are slower than the layer split (72.9 tok/s): the peer tier (`--peer-device`) 67.2, the helper cache
  (`--expert-cache-device1`) 53.8-59.3 (and 18.6 with `auto`, which filled the desktop card). Both meet at every layer
  through the host: 8.6-10.5 ms of every window. The cards themselves meet in 2.8-11.7 us when they signal each other
  directly (a 4-64 KB block into the other card's VRAM), ~0.6 ms per window for 48 layers. But measured before
  building it, the peer tier signalled card to card would reach only ~69-78 tok/s: the main card's own work per
  window (attention, recurrent state, projections, router, shared expert, head) is ~21 ms, the same as the split's
  two cards in turn, and the experts it hands off are ~2-3 ms of it. Not worth building. A tensor split's bound from
  a kernel trace is +15-40% (weeks of work); fusing the ~2,400 small kernels of a window helps one card and the split
  alike.

So for one conversation the cards take turns today, and the work went into making the turns shorter (section 2);
with several conversations both cards work at the same time (section 3). Expert parallelism does not beat the split
for one conversation on this model, even signalled card to card, because the experts are a small part of a token's
work; only a tensor split (weeks, +15-40% estimated) would put both cards on every part of it.

## 5. Fewer kernels per token (bench/results/2026-10-06-kernel-fusion-7900xtx)

A kernel in a HIP graph costs at least 2.86 us on these cards, and a decode window launches ~2,435 of them, two
thirds under 8 us; about a third of a layer's GPU time is the space between kernels. Merging the CPU's expert rows,
the GPU's and the combine into one kernel kept the same tokens and saved ~0.34 ms of a window (~1%,
`STRATA_COMBINE_GATHER=0` turns it off). Merging the indexer's appends saved nothing: they sit mostly in the commit,
which runs while that card is idle. Only kernels on the window's path from layer to layer count; the ceiling with
many more merges is estimated at ~5%.

## 6. On upstream's new main (bench/results/2026-10-07-upstream-port-7900xtx)

Upstream rewrote its `main` on 2026-10-06 (1,159 new commits). Branch `amd-7900xtx-port` carries our changes onto it.
Upstream as it was decoded 58.0 tok/s here; with our asynchronous commit and early launch 77.5; with its own new
`--pipeline-windows 2` - card 1 starts the next window on a guess while card 2 verifies this one, rolled back when the
guess is wrong - 92.7 tok/s (+25% over the same build without it). Eight conversations at once: 185.8 tok/s (151 on
the old branch). The early launch adds nothing on top of the pipeline but is +29% without it; the merged combine is
~0.5% there. The stream-fork switch and `k8v4` gave nothing.

## 7. Tuning one conversation on the port (07 Oct; bench/results/2026-10-07-upstream-port-7900xtx, autoresearch/explore-261007-tg)

Measured with the exact protocol (`STRATA_IQ_MT_MIN=1 --pcie-frac 0 --adapt-every 0`: the same text in every run of a
setting that does not change which experts sit where), then in the installed config.

| what | result | kept |
|---|---|---|
| the pipeline's gate at 0.1, `--spec 3 --spec-min-p 0.7` | +3.0%, then +5.6% (same text) | yes |
| the English + code draft subset (`--draft-vocab en`) | +3.0% (same text); installed config 96.8 tok/s | yes |
| the draft chain profiled | 312 kernels, 3.92 ms: the draft head 34%, attention 14%; the attention merge cut gave nothing | - |
| card 2's half profiled | ~938 kernels per window, a third of the time in the ~4 us gaps between them; no kernel stands out | - |
| the output head kept resident | bitwise equal; 1-3 columns 3-6% faster, 4 columns 30% slower | no |
| `HIP_FORCE_DEV_KERNARG=1` | no change | no |
| `STRATA_QFUSE=1` (upstream: the quantize written by its producer) | exact (`gr_parity` passes), ~+0.5%, inside the noise | yes |
| upstream's other exact gfx1151 switches | none faster; `TSUM`, `Q6_PACKED`, `Q8_PACKED`, `MMVF_ROWS`, `EXPERT_V2` 0.8-2.5% slower | no |
| a later layer split (28 / 30 / 32) | 18.4 -> 18.5 / 19.5 / 21.9 ms per window | no (26) |
| a reserve per card: `--vram-reserve-mib 700` (card 1) `--vram-reserve-later-mib 3072` (the desktop card) | exact protocol +7.4% (8 prompts, hit rate 82-94% -> 89-96%, no GTT spill); installed config the same window time (its PCIe share already serves the misses: 98-99% hit) | yes (no loss) |

**Where the pipeline loses time.** Card 1 runs window K+1 on the guess that K is accepted whole and that the target's
bonus token is the drafter's. Over 2,066 windows: 48.8% right, 37.9% all drafts accepted but the bonus wrong, 13.3%
fewer drafts accepted - the bonus is 74% of the discarded windows. With `STRATA_MTP_TOP2=1` (a diagnostic, off by
default) the drafter's second choice was the target's bonus in 31% of those. A kept speculative window takes ~11.5 ms,
a fresh one after a miss ~23 ms. Card 1 costs ~1.1-1.3 ms per extra row; after a wrong guess it idles ~12 ms (free),
after a right one it is already the later card (one extra row makes card 2 wait +0.94 ms). A second branch with the
second choice, only when the drafter's probability of its bonus is below 0.7, simulates to +5.8-7.4% - **but only if a
rescued window is as good as a kept speculative one**: drafts after the second choice and the next window prepared
from it, which needs a second chain on card 2's drafter branching at the bonus, and the branch's own attention cells on
card 1 (the KV cache is paged and streamed) and in the draft layer. A branch of the second choice alone (one row, no
drafts) gets ~9 ms per token on card 2 against ~11 ms for the fresh window it replaces, then still needs a fresh window
after it: ~2 ms saved per rescue against ~0.94 ms of tax on every right guess it rides along with - ~0% net. The full
version is weeks of work in the pipeline's core for ~+3-5%.

**From the literature** (autoresearch/explore-261007-tg/research.md): asynchronous draft / verify (AMUSD, PEARL,
PipeSpec), hybrid CPU / GPU experts (HybriMoE, kTransformers), windowed draft attention (Windowed-MTP) are already in
Strata; tree verification for Gated DeltaNet hybrids (TreeWY, STree) is what the second branch borrows; megakernels
(MPK, Hazy Research) would remove the gaps between kernels but are a rewrite; a self-distilled MTP head (FastMTP) is a
new draft model.

## 8. Next steps, not done

- **A second branch for the bonus token** (section 7) - built on branch `bonus-branch` (`STRATA_BONUS_BRANCH=1`, off
  by default) up to a lossless rescue: the branch rides in B's stage-0 window as slot rows (its own page in a spare
  K/V slot, its own indexer and PLE history, the conversation's GDN state); a branch the verdict confirms becomes the
  window and is promoted into the conversation at its commit. Exact protocol: the reference text in every run, the
  branch's hand-off bit for bit equal to B's when it is a copy of B (143 of 143). Speed: 1 row 20.6 s against 20.2 s
  of decode (73 rescues per run; a rescued window carries one token), 2 rows 21.8 s. Drafts after the second choice
  would need a second drafter chain, ready only after B could launch - B waits on every branched window. Not worth
  more work on this PC. The branch also works only below the attention selection's reach (8,192 cells).
- Found on the way: `STRATA_QFUSE=1` with batch windows (`"parallel"` slots) read a stale q8_1 image in the GDN
  layers (garbage text); upstream fixed the same (#1139), which the fork now uses.

## 9. Rebased onto upstream's main (08 Oct)

393 new upstream commits. Kept: the asynchronous commit, the early launch and its fixes, the pipeline's `go`, the
merged combine, setup's note, the diagnostic and the docs. Dropped as duplicates of upstream's own work: the batch
groups without pad rows, the server's groups and slot choice, their review fixes, the `STRATA_QFUSE` batch fix.
Checks with the rebased build: the same text as the engine before the rebase (pipelined and serial, exact
settings), our serial-path changes exact (the same text with them off), story + code 89.5 tok/s; eight prompts
105-108 tok/s against 104-108 (five pairs); `"parallel": 8` with upstream's batching 71.1 / 89.3 / 133.5 / 179.6
tok/s at 1 / 2 / 4 / 8 requests (ours: 72.1 / 88.7 / 133.0 / 185.8); `smoke.py` 7 of 7; `batch_test.py` the same
slots equal to solo as before. Pipelined and serial windows write different texts here with both engines
(upstream's behaviour). The fork's page: [FORK.md](../FORK.md).
- **MTP drafts in batch slots:** a conversation in a slot decodes one token per window (20-40 tok/s) against ~2 with
  drafts alone (~78 tok/s); the largest lever for a request that shares the cards.
- **Prelaunching the pipeline's graphs** (as the split does for one conversation): small here, the launches are 0.9 /
  0.34 ms of a ~21 ms window with 8 conversations.
- **The engine could report the pipeline groups it runs** (the server assumes the ones it asked for).

## Checks and tools

- Tests: ctest 61 of 65 on this PC, the same four failures before and after (a Q2_0 fixture, AVX-512, the
  container's memlock limit, a gfx1012-only signed-zero case); `tools/batch_test.py` and
  `tools/batch_interleave_test.py` identical to solo; `serve/test_parallel.py`, `serve/test_server.py`,
  `tools/test_setup_parallel.py` pass. `tools/test_setup_amd.py` (1) and `tools/test_setup_golden.py` (46) fail on
  this PC with and without these changes; the last step of `batch_interleave_test.py` (a prompt giving way) fails
  with the base engine too.
- Tools: `bench.py` / `show.py` / `smoke.py` (one conversation: speed, hashes, server checks) and `mbench.py`
  (several clients at once) in the two result folders; `vpy.py` runs a repository script with the setup's Python.
  Each folder's `iterations.tsv` lists every try in order with its numbers and the decision.
- Raw engine and server logs stayed on the PC (`data/logs/` is ignored by git); `data/results.jsonl` holds every
  measured run.

## 10. Long context up to 128K (09 Oct; bench/results/2026-10-09-long-context-7900xtx)

The production build through the server, to the configured 128K: a code word found at 10 / 50 / 90% of 8K, 32K, 64K
and 127K prompts (12 of 12); two of three facts spread through 34K / 63K / 119K combined right (3 of 3, thinking on);
~3,000-token reports after 32K / 67K / 122K of prompt coherent to the end at 101 / 95 / 92 tok/s; 9K-16K generated
tokens (a story and a program, greedy and sampled) coherent; a conversation grown to 96K over four parts recalled all
four codenames, each turn reading only its new part. No engine error, no GTT spill. Prompt reading 1,100-1,200 tok/s,
the first token after 108 s at 121K; beyond 32K the attention cache streams from RAM and 92-96% of its reads still hit
VRAM.

## 11. The ceilings (09 Oct; autoresearch/explore-261009-ceilings)

Measured: VRAM reads 917 GB/s per card (spec 960), every PCIe path 7.1 GB/s (3.0 x8), CPU RAM 45 GB/s; the engine's
expert GEMMs 31 TFLOPS (spec 123). A decode window reads ~2.8 GB on card 1 and ~2.6 GB on card 2 (3.1 / 2.9 ms at
bandwidth); the cards take 10.6 / 9.8 ms: ~29% of bandwidth each, with ~938 kernels per window per card whose launch
gaps (3-4 ms) equal the bandwidth floor. Roofline 314 tok/s serial, 606 overlapped; measured 105. Prompt reading:
practical compute ceiling ~4,200 tok/s, measured 1,100-1,460 (card 1 alone 1,880; the chunk cadence 2,050); card 1's
32K time is attention 16%, expert GEMMs 17% (at the ceiling), dequant 13-18%, host grouping 9-14%, combine 8%.
`STRATA_PF_FUSED=1`: 1,438 -> 2,057 tok/s on a 32K prompt (+45%; rounds differently, opt-in); `STRATA_PF_GEMM=1`,
16K chunks, `STRATA_PREFILL_HELP=1`: within 2%. The largest decode lever by the roofline is tokens per window (an
extra row costs ~1.2 of 10.6 ms), i.e. a better drafter; then fewer kernels per window.
