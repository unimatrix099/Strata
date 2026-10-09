# This fork: Strata on two AMD RX 7900 XTX

This is [unimatrix099/Strata](https://github.com/unimatrix099/Strata), a fork of
[Niko1221/Strata](https://github.com/Niko1221/Strata) used in production on one PC: two AMD RX 7900 XTX (24 GB
each, PCIe 3.0 x8, one of them drives the desktop), a Ryzen 9 5950X, ROCm 7.9, the IQ3_XXS model with a 128K
context. `main` is upstream's `main` plus the changes below; everything else in the repository is upstream's.

Every number here was measured on that PC. The full record, with every try and the dead ends, is in
[docs/RESEARCH_2X_7900XTX.md](docs/RESEARCH_2X_7900XTX.md).

## What the fork adds

| change | commit | measured |
|---|---|---|
| a split's commits do not wait (asynchronous commit on a two-GPU layer split) | `7b61b67` | +3.4% decode |
| card 2's window graph launched early, from a worker thread (`STRATA_SPLIT_EARLY_LAUNCH=0` turns it off) | `0419d49`, `a1d9116` | +29% decode without the pipeline, ~0 with it |
| upstream's `--pipeline-windows 2` raises card 2's `go` before its launch (without it a pipelined window timed out with the early launch) | `ef8af0c` | needed for the pipeline |
| the CPU's expert rows, the GPU's and the combine in one kernel (bit-exact; `STRATA_COMBINE_GATHER=0` turns it off) | `c45a551` | ~+0.5% |
| the pipelined draft chain's stream at the highest priority on ROCm too (before, it could share a hardware queue with card 2's windows) | `3e1b02c` | half the engine starts were ~10% slower (81-82 instead of 89-91 tok/s); now 6 of 6 fast |
| setup: on several cards the `"parallel"` note gives the measured trade-off | `bc94d9d` | - |
| `STRATA_MTP_TOP2=1`: a diagnostic, how often the drafter's second choice is the token a pipelined window missed (off by default) | `1ad25e4` | - |

Kept in the fork's docs: [docs/RESEARCH_2X_7900XTX.md](docs/RESEARCH_2X_7900XTX.md) (the history), the desktop-card
note in [docs/AMD_HIP.md](docs/AMD_HIP.md), the two-card sections in [docs/MULTI_GPU.md](docs/MULTI_GPU.md) and
[docs/BATCHING.md](docs/BATCHING.md), and the raw measurements with their scripts in `bench/results/2026-10-0*-*7900xtx`
and `autoresearch/`.

Dropped when the fork was rebased onto upstream's new `main` (08 Oct), because upstream had built the same thing:
the batch groups without pad rows, the server's pipeline groups and slot choice, their review fixes, and the
`STRATA_QFUSE` fix for batch windows (upstream #1139). Upstream's versions measured the same here (below).

## The production setup

`strata-iq3_xxs.json` (made by `setup.py`, then tuned):

- `--gpus 1,0`: the card without the desktop runs the first stage; `--vram-reserve-mib 700 --vram-reserve-later-mib
  3072`: the desktop card keeps 3 GB free (with less, the driver moves ~20 GB of Strata's memory to system RAM and
  decoding drops to 15 tok/s - docs/AMD_HIP.md);
- `--pipeline-windows 2 --spec 3 --spec-min-p 0.7`, `"draft_vocab": "en"`;
- `--conversation-cache-mib 16384 --conversation-cache-slots 4` (since 09 Oct): up to four conversations parked in
  RAM, so switching between long ones takes ~1 s instead of re-reading them (~55 s at 64K);
- `"env": {"STRATA_PIPELINE_DEBUG": "1", "STRATA_PIPELINE_THETA": "0.1", "STRATA_QFUSE": "1", "STRATA_PF_FUSED": "1",
  "STRATA_HIP_WMMA": "1"}` (the pipeline's gate is read only with the debug switch; `STRATA_PF_FUSED` = upstream's
  fused prompt path and `STRATA_HIP_WMMA` = its prompt attention on the matrix cores, both since 09 Oct: prompts read
  ~2-3x faster at 16K-121K, the first token at 118K after 36 s instead of 101-108; both round differently from the
  default path, checked with the long-context suite).

The engine is `engine/strata`, built from `main` (below). Measured on 08 Oct with the build of `main`:

| | tok/s |
|---|---|
| one conversation, story + code (greedy, 512 tokens) | 89.5 (exact-text settings) |
| one conversation, eight prompts | 105-108 (installed config) |
| several at once, `"parallel": 8`, total at 1 / 2 / 4 / 8 requests | 71.1 / 89.3 / 133.5 / 179.6 |
| prompt reading, 4.7K tokens | ~1,100 |

Long context (09 Oct, [bench/results/2026-10-09-long-context-7900xtx](bench/results/2026-10-09-long-context-7900xtx/README.md)):
recall at 8K-127K 12 of 12, reasoning over facts spread through 34K-119K 3 of 3, ~3K-token answers after 122K of
prompt coherent at 92 tok/s, 9K-16K generated tokens coherent, a 96K-token conversation recalled - no garbage or
drift up to the 128K limit; prompt reading ~1,100-1,200 tok/s, first token after 108 s at 121K (a speed table by
prompt and output size: the folder's "Speed summary"). Several 64K conversations at once (`"parallel"` 2 and 4): every conversation right and kept apart, but slower in
all than one at a time (16.7 tok/s in all for 4). Switching between three 64K conversations: 50-58 s per follow-up without upstream's conversation cache,
0.7-1.3 s with `--conversation-cache-mib 16384 --conversation-cache-slots 4` (the same answers; 1.5-2.4 GB of RAM per
parked conversation).

## Docker (AMD)

`Dockerfile.rocm` + `docker-compose.rocm.yml` + one `.env` file (`docker/rocm.env.example`) run the production setup in
a container, with the tuned settings as env vars: [docs/DOCKER_ROCM.md](docs/DOCKER_ROCM.md). Written 09 Oct, not yet
built or run (to be tested on the PC). Its ROCm: setup.py's pinned ROCm 7 by default. Measured here against ROCm 7.9: 10.1 not
recommended (GPU memory faults with the conversation cache, no speed gain), 7.14.1 stable but prompts ~40% slower.

## What was tested (summary, 09 Oct)

All on this PC (2x RX 7900 XTX, IQ3_XXS); the details are in the linked folders and in
[docs/RESEARCH_2X_7900XTX.md](docs/RESEARCH_2X_7900XTX.md).

**Speed, one conversation**

| test | result |
|---|---|
| upstream as it was (06 Oct) -> production `main` | 58.0 -> ~97 tok/s (story + code); 105-108 tok/s over eight prompts |
| the asynchronous commit + early launch (two cards, no pipeline: upstream's default) | 54.8 -> 74.5 tok/s, +36% |
| pipeline tuning (gate 0.1, `--spec 3 --spec-min-p 0.7`, English draft vocabulary) | ~+8%, then +3% |
| the drafter's stream priority on ROCm | before: about every other engine start ~10% slower (81-82 instead of 89-91 tok/s); after: 6 of 6 starts fast |
| tried, no gain | later layer split, upstream's gfx1151 switches, a resident head kernel, `HIP_FORCE_DEV_KERNARG`, the VRAM reserve (installed config), the second branch for the bonus token |

**Correctness of the code changes**

| test | result |
|---|---|
| exact-text runs, every change on and off | the same text in every combination |
| after the rebase onto upstream (393 commits) | the same text and speed as before |
| server smoke (facts, 4.5K prompt, Anthropic API, a cancelled stream, two turns, sampled) | 7 of 7, repeatedly |
| `tools/batch_test.py` (4 conversations against their solo runs) | as upstream: the known small drift in 2 of 4, with and without the fork's changes |
| bugs found | `STRATA_QFUSE` with `"parallel"` slots gave garbage text (upstream fixed it, #1139); half the pipelined engine starts slow (fixed here) |

**Long context, one conversation, to 128K** ([bench/results/2026-10-09-long-context-7900xtx](bench/results/2026-10-09-long-context-7900xtx/README.md))

| test | result |
|---|---|
| a code word at 8K / 32K / 64K / 127K, depths 10 / 50 / 90% | 12 of 12 found |
| facts spread through 34K / 63K / 119K, combined | 3 of 3 |
| ~3,000-token reports after 32K / 67K / 122K of prompt | coherent to the end, 101 / 95 / 92 tok/s |
| 9K-16K generated tokens (a story and a program, greedy and sampled) | no drift or garbage (the two flags were false alarms, read) |
| a conversation grown to 96K over four turns | all four codenames recalled; each turn read only its new part |
| prompt reading | 1,100-1,200 tok/s to 121K; the first token after 27 s at 32K, 51 s at 64K, 108 s at 121K |
| stability | no engine error, no GTT spill |

**Several conversations at once**

| test | result |
|---|---|
| short prompts, `"parallel": 8` | 71 / 89 / 133 / 180 tok/s in all at 1 / 2 / 4 / 8; the 8th answer after ~5 s instead of 51 s |
| 2 and 4 conversations of 64K each | all right and kept apart, no garbage; slower in all than one at a time (16.7 tok/s for 4) |
| one wrong sum (8128 for 8228) | the model's arithmetic without thinking, the same alone; with thinking on always right |

**Switching between long conversations**

| test | result |
|---|---|
| three 64K conversations, nine alternating follow-ups | without the conversation cache 50-58 s per switch; with it 0.7-1.3 s, the same answers (9 of 9) |
| its cost | 1.5-2.4 GB of RAM per parked 64K conversation |

**Not tested:** contexts above 128K, more than 4 long conversations at once, other languages than English, images.

**Use:** `"parallel"` for short-context agents; long contexts one at a time, with the conversation cache for switching
(on in the production config).

Ceilings (09 Oct, [autoresearch/explore-261009-ceilings](autoresearch/explore-261009-ceilings/README.md)): VRAM 917 GB/s
per card, PCIe 7.1 GB/s, RAM 45 GB/s; decode runs at ~17-30% of its bandwidth roofline (~938 kernels per window per
card: the launch gaps equal the bandwidth floor), prompt reading at a third of the practical compute ceiling;
`STRATA_PF_FUSED=1` read a 32K prompt +45% faster (not bit-identical, quality to check before production).

## History

| date | what | result |
|---|---|---|
| 05 Oct | installed on the two cards; the desktop card's memory spill found and fixed (`--gpus 1,0`, 3 GB reserve) | decode 15 -> 56-68 tok/s |
| 05 Oct | decode on the split: asynchronous commit, early launch | 55.0 -> 72.2 tok/s |
| 05-06 Oct | several conversations at once: pipelined batch groups, server groups | 8 at once 70.9 -> 151 tok/s |
| 06 Oct | expert parallelism, card-to-card experts, spin-waiting, kernel merging: measured, not kept | - |
| 06-07 Oct | ported onto upstream's rewritten `main`; upstream's `--pipeline-windows 2` | 92.7 tok/s; 8 at once 185.8 |
| 07 Oct | the pipeline tuned (gate 0.1, `--spec 3 --spec-min-p 0.7`), the English draft vocabulary | 96.8 tok/s |
| 07 Oct | card 2 profiled, `STRATA_QFUSE`, upstream's gfx1151 switches, layer split, VRAM reserve per card, online research | small or no gains |
| 07 Oct | a second branch for the drafter's second choice, built to a lossless rescue (branch `bonus-branch`) | no gain on this PC |
| 08 Oct | rebased onto upstream's `main` (393 new commits); duplicates of upstream's own work dropped | same text, same speed |
| 08 Oct | the drafter's stream priority on ROCm (found while preparing `pr/two-gpu-split`) | no more slow engine starts |
| 09 Oct | long-context quality and speed up to 128K | no garbage or drift; recall and reasoning all right |
| 09 Oct | the hardware ceilings and the roofline for decode and prompt reading; prompt-path switches measured | `STRATA_PF_FUSED=1` +45% on prompts (not exact) |
| 09 Oct | `STRATA_PF_FUSED=1` in production after the full quality suite | prompts 1.8-2.3x faster at 16K-121K |
| 09 Oct | `STRATA_HIP_WMMA=1` in production after a quality check | +20% more on prompts; 118K read at 3,368 tok/s |

## Bringing in upstream's changes

`main` follows upstream by **merging** (not rebasing): the fork's history stays as it is, and a merge shows exactly
what came in.

```
git remote add upstream https://github.com/Niko1221/Strata.git    # once
git fetch upstream
git checkout main
git merge upstream/main          # conflicts are usually in src/core/verify.cpp and src/program/generate.cpp
```

Before pushing, check on the PC:

1. Build: `python autoresearch/explore-261007-tg/bld.py strata` (the build directory `build-hip`; that script runs
   the setup's ninja), and the setup tests: `python tools/test_setup_parallel.py`.
2. Exactness: `bench/results/2026-10-05-split-decode-7900xtx/bench.py --label X --exe build-hip/strata --env
   STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0` - the same text (`shas`) as the
   installed engine, and with `--env STRATA_SPLIT_EARLY_LAUNCH=0`. (Pipelined and serial windows write different
   texts on this PC; that is upstream's behaviour, not a fault.)
3. Speed: `autoresearch/explore-261007-tg/bench_many.py --label X --exe build-hip/strata` against the installed
   engine, alternating, several runs (compare `summ.py`'s ms per window: a change in which experts sit where
   changes the text and the tokens per window).
4. Server: `bench/results/2026-10-05-split-decode-7900xtx/smoke.py` (7 checks) and `tools/batch_test.py --exe
   build-hip/strata --config strata-iq3_xxs.json --batch 4 --n 4 --extra "--layer-split 26"`.
5. Install: copy `build-hip/strata` to `engine/strata` (keep the old one beside it).

If upstream builds something this fork has, prefer upstream's version and drop the fork's (as on 08 Oct): fewer
differences, easier merges.

## Branches

- `main`: production (this page).
- `pr/two-gpu-split`: upstream's `main` plus only the measured improvements (the hand-over changes, the drafter's
  priority, setup's note, a doc section), for a pull request to upstream; two cards without the pipeline 54.8 ->
  74.5 tok/s (+36%) on its base.
- `bonus-branch`: the second-branch experiment (`STRATA_BONUS_BRANCH=1`), lossless, no gain; kept for reference.
- `amd-7900xtx-port`, `split-decode-speed`, `multi-agent-batch`, `hip-rocm79-alignas-fix`: earlier stages of the
  work, superseded by `main`; `old-main-2026-10-04`: the fork's `main` before 08 Oct (an old copy of upstream).
