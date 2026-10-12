# The proposal split into three branches, each against current upstream (12 Oct)

`pr/amd-decode-speed` (the fork's engine changes on `upstream/main` 7695859a) was trimmed and split into three
branches for upstream, one change each. Every new default is now HIP-only, so a CUDA build behaves as upstream's.

| branch | what it changes | default |
|---|---|---|
| **A** `pr/amd-split-handover` (d3069be8, on upstream) | two cards: the next stage's graph launched early, the first stage's commit asynchronous; the MTP chain's stream at high priority on HIP (as on CUDA) | on with HIP; CUDA unchanged (`STRATA_SPLIT_EARLY_LAUNCH=1`, `STRATA_SPLIT_COMMIT_ASYNC=1` opt in) |
| **B** `pr/amd-copy-combine` (fe866ebe, on upstream) | the PCIe copy job on 96 blocks instead of 384; the CPU rows, GPU rows and combine of the doorbell path in one kernel | on with HIP; CUDA keeps 384 blocks and the three kernels (`STRATA_COMBINE_GATHER=1` opts in) |
| **C** `pr/amd-ffn-fusion` (3078c794, on B) | `STRATA_HIP_POST_SPLIT`, `STRATA_HIP_FRONT_FUSE`, the kernel bodies over a VBlock, `ffn_half_bench` | off; both switches compile to off outside HIP; the bench builds only with HIP |

Removed from all three: the research diagnostics (`STRATA_MTP_TOP2`, `STRATA_PL_LAUNCH_TIMING`, the split's launch
and sync timers, `NATIVE_EXPERT_BENCH_GY`). Merging A into C gives exactly the trimmed `pr/amd-decode-speed`
(c4d01e47; `git diff` empty).

**Builds:** each branch for gfx1100 (`STRATA_PREFILL_MMQ=ON`, as setup builds) and with the gfx906 compat backend,
which compiles the CUDA code paths through HIP: all six pass. CUDA itself (nvcc) and SYCL were not built here.

All runs: Swift 1.5 IQ3_S on RX 7900 XTX (one card: setup's one-GPU config; two cards: the fork's tuning without
the two switches, `strata-swift-iq3_s-2gpu-base.json` in `../2026-10-12-upstream-vs-branch`). Scripts: `s1.sh`,
`r1.sh`-`r4.sh`; every result in `data/results.jsonl`.

## Same text

The exact protocol (`STRATA_IQ_MT_MIN=1 --pcie-frac 0 --adapt-every 0`, two cards also `STRATA_PF_FUSED=0
STRATA_HIP_WMMA=0`), story and code prompts, 512 tokens:

| | one card | two cards, pipelined windows | two cards, without |
|---|---|---|---|
| upstream | 35 of 36 requests the reference text (one code run differed, 9th repeat) | the reference (earlier runs) | 20 of 20 |
| A | - | 4 of 4 | 19 of 20 (its first story differed once; then 16 of 16, and 32 of 32 with either part off) |
| B | 39 of 40 (one story differed, 2nd repeat; then 36 of 36) | 4 of 4 | - |
| C, switches off / on | 4 of 4 / 4 of 4 | 4 of 4 / 4 of 4 | - |

A rare one-off text difference happens on upstream too, at the same rate; no branch made a difference that repeated.

## Decode speed

Eight prompts per round, normal settings. Mean of all prompts, and the per-prompt ratio to upstream (geometric mean):

| | rounds | tok/s | vs upstream | prompts faster |
|---|---|---|---|---|
| one card, upstream | 2 | 65.3 | | |
| one card, B | 2 | 67.4 | **+3.2%** | 7 of 8 |
| one card, C + switches | 2 | 69.0 | **+5.6%** | 8 of 8 |
| two cards (pipelined windows), upstream | 5 | 102.9 | | |
| two cards (pipelined), A | 5 | 102.3 | -0.6% (noise) | 5 of 8 |
| two cards (pipelined), B | 5 | 104.7 | +1.7% (within the rounds' spread) | 5 of 8 |
| two cards (pipelined), C + switches | 2 | 111.9 | **+9.0%** | 8 of 8 |
| two cards **without** pipelined windows, upstream | 3 | ~57 (medians 57.5 / 57.6 / 56.6) | | |
| two cards without, A | 3 | ~86.5 (medians 83.4 / 87.9 / 88.4) | **about +50%** | 8 of 8 |

With `--pipeline-windows 2` (the fork's tuning) the windows go through the pipeline's own launches, so A's early
launch and asynchronous commit do nothing there. Without it, A is by far the largest change. Upstream turns pipelined
windows on by default only for gfx12 splits of 3+ stages, so a two-card RX 7900 XTX on upstream's defaults runs
without them. Which of A's two parts does it, exact protocol, two cards, no pipelined windows (8 repeats each, all the
reference text):

| | story tok/s | code tok/s |
|---|---|---|
| upstream | 48.5 | 61.4 |
| A | 60.7 | 84.7 |
| A, early launch off | 50.3 | 63.6 |
| A, synchronous commit | 56.3 | 76.5 |

The two parts need each other: early launch alone gets about half, the asynchronous commit alone little.

Server smoke (7 checks: facts, a 4.5K prompt, the Anthropic API, streaming, cancel, two turns, sampling), C with
both switches on two cards: 7 of 7, no engine errors.

Not tested here: NVIDIA cards (CUDA is unchanged by construction, not by a run), RDNA2/RDNA4 or gfx906 hardware
(compiled only), more than two cards.
