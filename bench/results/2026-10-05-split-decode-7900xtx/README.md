# Decode on a two-GPU layer split: 2x RX 7900 XTX (2026-10-05)

**Result:** decode on two RX 7900 XTX went from **55.0 to 72.2 tok/s** (+31%, median of a story and a code prompt, 3
repeats each), with the same tokens. Two changes did it, both only on the layer-split path:

1. **Asynchronous commit on a split** (`dd2c0e5`): 55.0 -> 57.5 tok/s.
2. **Early launch of the later stage's window graph** (`43a37bf`): 57.5 -> 72.2 tok/s.

One GPU never reaches either change; its decode measured the same (59.3 vs 59.7 tok/s). Prompt reading is unchanged
(1,241-1,246 vs 1,245 tok/s).

## Rig and setup

- 2x AMD Radeon RX 7900 XTX (gfx1100, 24 GB each), both on PCIe 3.0 x8 (the engine's probe reads 7.1 GB/s); HIP
  device 0 (PCI 0a) drives two monitors.
- Ryzen 9 5950X (16 cores, AVX2, no AVX-512), 126 GB RAM, Linux in a container, system ROCm 7.9.0 (hipBLASLt 1.1.0).
- Engine 0.1.39 from this branch, built by `./setup.sh --backend hip`.
- Model: Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS, 128K context, `--kv int8 --kv-resident 32768`, MTP `--spec 4
  --spec-min-p 0.5`, expert profile, `--gpus 1,0 --vram-reserve-mib 3072` (the installed `strata-iq3_xxs.json`).
  Auto split: layers 0-25 on HIP device 1, 26-47 and the head on HIP device 0; 83% of the experts in VRAM.

## Method

`bench.py` starts `serve/server.py` with a variant of the installed config (extra engine flags, environment, GPU
list, another engine binary), sends one warm-up, then a story prompt and a code prompt 3 times each (greedy, 512
tokens; the model thinks, so most tokens are reasoning), and records decode tok/s, draft acceptance and a hash of the
reasoning + answer text. `show.py` prints the results (`data/results.jsonl`); engine and server logs are in
`data/logs/`.

- **Speed:** the median of the 6 answers ("all").
- **Exactness:** with `--adapt-every 0`, the first story and code answer after a fresh start are reproducible (the
  base binary gave the same hashes twice). A change that should only move timing must keep both hashes. Later
  repeats can differ: suffix drafts and the prompt cache depend on the earlier answers, which changes the window sizes.
- **Guards:** one GPU within 2% of the base binary; ctest unchanged; `smoke.py` (server checks) passes.
- **Where the time goes:** `STRATA_DECODE_TIMING=1` and `STRATA_SPLIT_TIMING=1` (per stage: wait for the GPU, pool +
  plan, commit, and since this branch the graph launch and the final sync), and one `rocprofv3 --kernel-trace` run.

## Where the time went (before)

Per decode window (~39.6 ms, ~2 tokens):

| part | ms |
|---|---|
| stage 0 (26 layers): waiting for its GPU | 16.1 |
| stage 1 (22 layers + head): waiting for its GPU | 14.0 |
| stage 1: graph launch, after stage 0 finished | 1.25 |
| commits (synchronous, one stage after the other) | 1.0 + 0.6 |
| MTP draft | 2.4 |
| pool + plan (CPU experts), commit/emit, rest | ~4 |

The stages run one after the other, so only one card works at a time. One card alone decoded **faster** than the
split (60.1 vs 54.2 tok/s): it spends 6-12 ms per window on the CPU's experts, but no hand-off.

`rocprofv3` (one run each, decode only): `wait_flag_ge_kernel` (a GPU spinning on a host word) is ~85% of all kernel
time; the real work is ~1,200 kernels per stage per window, ~11 ms of GPU compute per stage per window.

## Kept

**1. Asynchronous commit on a split.** 42b4299 made the commit graph asynchronous on one GPU (+5.6% there) but kept
the wait on a split, because what followed a commit then synchronized only one device. Since a454dbb every point that
touches a session waits on the stages' commit events (`wait_commit` walks the chain), so a split's stages now return
at once too; `--split-device 0` (two stages sharing one session on one GPU) keeps the wait.

| fresh start, `--adapt-every 0` | story | code | hashes |
|---|---|---|---|
| base | 50.6-51.1 | 52.1-52.4 | 27f48d / 0a86d3 |
| async commit | 53.4 | 54.9 | the same |

**2. Early launch of the later stage.** The later stage's graph (~1,200 nodes; `cudaGraphLaunch` takes 1.25-1.5 ms
of CPU on ROCm) was launched only after stage 0 had finished and synchronized. Now a worker thread launches it right
after stage 0's own launch; the graph waits on a mapped word `go` before it reads its inputs or the hand-off, and
stage 1's `run` only raises that word once stage 0 has synchronized. Stage 0's GPU wait fell 14.4 -> 11.2 ms and stage
1's 12.2 -> 8.0 ms per window.

Part of the gain is clocks: RDNA3 lowers its shader clock in the idle gaps between windows. With the early launch,
stage 1's card spins in the graph's first kernel while stage 0 runs and stays clocked up (busy samples during decode,
median shader clock):

| | stage 0's card | stage 1's card |
|---|---|---|
| early launch off | 1,490 MHz | 1,525 MHz |
| early launch on | 1,870 MHz | 2,948 MHz |

| full protocol (adaptation on, 3 repeats) | story | code | all |
|---|---|---|---|
| base binary | 51.3 | 64.3 | 55.0 |
| async commit only (`STRATA_SPLIT_EARLY_LAUNCH=0`) | 53.3 | 66.0 | 57.5 |
| **both** | **66.8** | **83.7** | **72.2** |
| both, `--gpus 0,1` (the desktop card first, 3 GB reserve) | 65.5 | 83.3 | 74.5 |

The first story and code answers are identical in all of them (hashes 4f4ed5 / 4ce8bd), and with both changes all
later repeats are identical between runs too.

Per window after: ~28-32 ms = stage 0 launch 1.6 + GPU 12.1 + pool 0.6, stage 1 GPU 9.0 + pool 0.8, head 1.2, draft
2.1-2.5, commit/emit 0.2.

## Tried and dropped

| try | result |
|---|---|
| keep stage 0's card busy between windows (a time-limited spin kernel on a side stream) | 71.4 vs 71.7 without; stage 0's GPU wait unchanged (11.8 ms) |
| HIP graph runtime switches: `DEBUG_HIP_GRAPH_BATCH_SIZE` 16/64/256, `DEBUG_CLR_GRAPH_PACKET_CAPTURE=0`, `HIP_FORCE_DEV_KERNARG=1`, `DEBUG_HIP_KERNARG_COPY_OPT=0` | 65.2-66.6 vs 68.5 (fresh start); launch 1.46-1.74 ms, unchanged |
| the default 700 MiB reserve on both cards (94% resident) | the desktop card moved 19.4 GB to GTT mid-run: 15-20 tok/s |
| `--spec 6` / `--spec 5` / `--spec-min-p 0.3` / `0.7` | 71.5 / 69.3 / 70.0 / 68.5 vs 72.2 |
| split point K=20 / K=22 (auto: 26) | 69.2 / 67.3 vs 72.2 |

## Checks

- **One GPU** (`--gpus 1`): 59.3 (this branch) vs 59.7 tok/s (base). On one GPU the first answers are not
  reproducible even with the base binary (which experts the CPU or PCIe serves depends on timing), so it is checked
  by speed only.
- **ctest:** 61 of 65, the same four failures as before (`ple_parity` needs the Q2_0 fixture, `expert_multi_test`
  an AVX-512 CPU, `platform_memory_test` a memlock limit the container lacks, `hip_q2_zero` the gfx1012-only
  signed-zero case).
- **Server smoke** (`smoke.py`): 7/7 - a fact question, a 4.6K-token summary (1,062 tok/s prompt, 80.6 tok/s
  decode), the Anthropic endpoint, a stream the client left after 3 s followed by a new request, a two-turn chat, a
  sampled answer; no error lines in the engine log.
- **Prompt reading:** 1,241 / 1,246 tok/s (this branch, two runs) vs 1,245 (base), ~4.6K-token prompts.

## Review and the failure paths

An independent code review of both commits (read-only, adversarial) confirmed the asynchronous commit (every later
use of a session waits on the stages' commit events or syncs the right device), the memory ordering between the main
thread and the worker, the teardown order and that one GPU is untouched. It found one bug and three risks in the early
launch's failure paths, all fixed in the follow-up commit:

- **Bug:** when stage 0 failed after posting, the guard raised only `go`; stage 1's graph then spun at its first
  layer flag (the worker had reset it), so a #267 timeout could leave GPU B spinning. The guard now raises every word
  that stage's spin kernels wait on.
- A release while the worker was between staging and launching could be undone (it resets the words): the worker
  now checks for a release before it launches, and the release waits for a job in flight.
- Stage 1's early returns (window size, context end, an earlier release) ran before it took the early job's state,
  which could later raise `go` for a stale graph: the state is taken first now, and a launched graph is released on
  those returns.
- An exception on the worker would have ended the process (`std::terminate`): it is now a failed job, reported by
  the stage's `run`.

**Failure test:** `STRATA_TEST_VERIFY_STALL=30` (the #267 hook: window 30 never finishes on stage 0) - the stall
report came after 60 s, the release reached the early-launched graph on stage 1 too ("the GPU finished in 13 ms"),
and both cards were idle afterwards. After the fixes: the same hashes, 71.1 tok/s (72.2 before, within noise), smoke
7/7, ctest unchanged.

## Switches

Both changes are on for a split across GPUs and do nothing on one GPU:

- `STRATA_SPLIT_COMMIT_SYNC=1` - the old synchronous commit on a split.
- `STRATA_SPLIT_EARLY_LAUNCH=0` - no early launch.
- `STRATA_SPLIT_TIMING=1` - per-stage timing, now with the graph launch and the final sync.

Caveat: an early-launched graph occupies its stream (and the hardware queue ROCm maps it to) while it waits. Nothing
else runs on that card during that time in a plain split; helper caches (`--expert-cache-remote`) on a card that also
runs a stage were not tested.

## Reproduce

```sh
python3 bench/results/2026-10-05-split-decode-7900xtx/bench.py --label mine            # the installed config
python3 bench/results/2026-10-05-split-decode-7900xtx/bench.py --label off --env STRATA_SPLIT_EARLY_LAUNCH=0
python3 bench/results/2026-10-05-split-decode-7900xtx/bench.py --label one --gpus 1     # one card
python3 bench/results/2026-10-05-split-decode-7900xtx/bench.py --label pf --prefill     # prompt reading
python3 bench/results/2026-10-05-split-decode-7900xtx/show.py
python3 bench/results/2026-10-05-split-decode-7900xtx/smoke.py
```
