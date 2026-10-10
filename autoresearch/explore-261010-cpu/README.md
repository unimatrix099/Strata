# CPU-side optimizations on the 5950X (10 Oct)

The question: what does the CPU (Ryzen 9 5950X, 16 cores / 32 threads in two CCDs of 8 with a 32 MB L3 each, AVX2,
128 GB DDR4 at ~45 GB/s) do in decode and in prompt reading, and what can be made faster? Production config, 2x RX
7900 XTX, IQ3_XXS. Scripts here (`c1.sh` ... `c5.sh`, `f4.sh`), results in `*.jsonl`.

## What the CPU does (a read of the code)

- **Decode.** One pipeline thread, pinned to CPU 0, serves both cards: it polls each layer's doorbell in mapped
  memory, builds the expert plan (~1-3 us), launches the window graphs (`cudaGraphLaunch`), polls the draft chain and
  runs the adaptive tier. The expert pool has 15 workers (CPUs 1-15, one per core, both CCDs), spinning between
  layers; a layer's CPU experts are split into 48 row-block tasks per phase. Measured (`STRATA_DECODE_TIMING=1`):
  **0.19-0.25 CPU experts per layer per window** (the VRAM cache hits 98.7-99.0% of expert lookups), the CPU's expert
  work ~0.46 ms per ~15.7 ms window. Four of five layers need no CPU expert at all.
- **The window launches**, a new diagnostic (`STRATA_PL_LAUNCH_TIMING=1`, verify.cpp): each pipelined window's
  `cudaGraphLaunch` holds the pipeline thread for a **median 150-172 us, p90 207-257 us, at times 1.6-3.4 ms**, on
  both cards - while no doorbell is answered.
- **Memory.** The 40 GB expert arena is fully in 2 MB pages (`AnonHugePages` 40.0 GiB in the engine's smaps); the
  PLE table is a 4 KB-page file mapping, but decode reads only a few rows of it per window. Nothing is page-locked in
  the sandbox (`ulimit -l` 8 MB): Docker sets memlock unlimited.
- **Prompts.** `STRATA_PREFILL_CPU_SHARE=1` (production since 09 Oct) makes chunks below
  `STRATA_PREFILL_CPU_SHARE_MAX` (default 3072 tokens) stage their experts after routing and hand the CPU pool the
  non-resident experts with few tokens; longer chunks stream every expert as before.

## Measured

| try | result |
|---|---|
| `--pool-workers 7` (all workers on CCD0 with the host) | 0.961, 0.965, 0.936 - **-4.6%**, 4 of 24 prompts faster |
| `--pool-tasks 16` (default 48) | 0.960, 1.036, 0.972 - -1.1% |
| `--pool-tasks 32` | 0.959, 1.015, 0.957 - -2.3% |
| the window launch on a launcher thread of its own (built as `STRATA_PL_ASYNC_LAUNCH=1`, then removed) | exact text the same, eight prompts 0.966, 0.988, 0.961 - **-2.8%**; the launch overlaps GPU work that runs anyway, and the hand-off costs more than it frees |
| the prompt path's CPU share: `1` against `auto` and off, 1K / 2K / 4K | 1K 691 / 678 / 606 tok/s, 2K 1,050 / 1,048 / 979, 4K 1,456 / 1,457 / **1,502** |
| share `1` against off at 4K / 16K / 32K | 4K 1,400 / 1,436, 16K 2,364-2,534 / 2,571, 32K 2,999-3,015 / 3,044-3,055: **the armed share cost every prompt from 4K up 1.5-3%** |
| the share's limit (two rounds each; off / limit 3,072 / **2,048** / 1,024) | 1K 605 / 691 / **693** / 675; 2K 977 / 1,050 / **1,052** / 976; 4K 1,505 / 1,459 / 1,481 / 1,505; 16K 2,533 / 2,549 / 2,570 / 2,592 |

**Kept: `STRATA_PREFILL_CPU_SHARE_MAX=2048`** in the production config (`strata-iq3_xxs.json.before-sharemax` is the
previous one): the 1K and 2K gains stay (+14.5% and +7.7% against off), 4K +1.5% against the old limit; 16K differs
within the run-to-run spread (the C4 rounds put off ahead, C5 put it behind). A 4K prompt keeps ~1.6% against off -
part of it is read in a chunk below 2,048 tokens. The share already ran on every chunk below 3,072 since 09 Oct (checked
with the long-context suite then), so this narrows a checked setting; chunks that no longer share take the default path.

## Not done, and why

- **Prefetching the next layer's likely CPU experts into the L3** while the workers wait: at 0.2 CPU experts per
  layer-window the CPU's expert time is ~3% of a window; the most this could return is about that, less the misses it
  guesses wrong.
- **Fewer pool barriers per layer** (4 today): the same small base.
- **The PLE gather on layer 0's flag C** (16 rows x T at the first layer's answer): a few us per window.
- **Host settings** (root on the host, not possible from the sandbox): the `performance` governor (today `schedutil`,
  boost on) - 15 spinning cores and a single-threaded pipeline thread are what a governor ramps badly; and
  `ulimit -l unlimited` so the PLE table and the arena are locked. Both cheap to try on the host: the eight-prompt
  bench before and after.

So on this machine the CPU is not where decode time goes any more: the cards wait on it ~0.2 CPU experts per layer,
and the one thread that serves them is busy launching graphs for ~0.3 ms per window, which moving off it did not help.
