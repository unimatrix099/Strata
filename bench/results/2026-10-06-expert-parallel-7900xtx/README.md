# Experts of one conversation on both cards: 2x RX 7900 XTX (2026-10-06)

**Question:** instead of the layer split (card A runs layers 0-25, then card B runs 26-47, so they take turns), can
both cards work on the same layer at the same time - the routed experts split between them (expert parallelism), or
every matrix split like llama.cpp's `-sm tensor` (tensor parallelism)?

**Answer, measured:** Strata's two expert-parallel modes run on AMD, but both are slower than the layer split here,
because they meet at every layer through the host (8.6-10.5 ms of every decode window). The cards themselves can meet
in 3-12 us when they signal each other directly, so expert parallelism with the host out of the per-layer path is the
promising design; it does not exist yet.

Same PC and model as bench/results/2026-10-05-split-decode-7900xtx (2x RX 7900 XTX on PCIe 3.0 x8, Ryzen 9 5950X,
ROCm 7.9.0, IQ3_XXS, 128K context, MTP), engine from the `multi-agent-batch` branch, measured with that folder's
`bench.py` (greedy story + code prompts x3, median decode tok/s; results in its `data/results.jsonl`).

## One conversation, decode

| mode | tok/s | ms per window | main card waited for | host per window, per layer |
|---|---|---|---|---|
| layer split (default) | **72.9** | ~28-30 | 12.1 + 9.0 (in turn) | ~1.4 |
| one card | ~60 | 34-41 | 16.5-17 | 6-12 (the CPU's experts) |
| helper cache, `--expert-cache-device1 auto --remote-expert-opt` | 18.6 | - | - | - |
| helper cache, 11,000 experts, `--remote-expert-opt` | 53.8 | 37.7 | 20.8 | 10.0 |
| helper cache, 11,000 experts, plain | 59.3 | 41.9 | 18.8 | 10.5 |
| peer tier, `--peer-device 1 --peer-reserve-mib 3072` | 67.2 | 29.7 | 14.3 | 8.6 |

- **Helper caches** (`--expert-cache-device1`, docs/SECOND_GPU.md, measured on NVIDIA so far): the main card runs all
  48 layers, the helper computes the experts it holds, and each layer waits for its rows through pinned host memory.
  With `auto` the helper took 23.44 GiB of the desktop card, keeping only its own 512 MiB free (it does not follow
  `--vram-reserve-mib`), and the requests stalled (6-30 tok/s); with a fixed 11,000 experts the desktop card stayed
  in VRAM (601 MiB GTT, the desktop's own). `--remote-expert-opt` (measured +63-132% on dual RTX 4090 over the plain
  helper) was slower than the plain helper here.
- **The peer tier** (`--peer-device`): the second card holds the experts the first does not (12,751 experts, 20.77
  GiB, 2,502 MiB left free on the desktop card; with the first card's 9,400 that is 22,151 of 24,576 profiled pairs
  on GPUs) and computes their rows each layer. In decode the activations and the rows go through pinned host memory,
  not P2P (P2P itself works between the two cards: below). The best of the expert-parallel modes, 8% below the layer
  split: per window
  the main card's GPU waits 14.3 ms (less than the split's 21.1 in turn), but the host spends 8.6 ms at the layers -
  it launches the peer's share (two copies, a quantize, the expert kernel, a scatter to host rows) and spins until it
  is done, 48 times.
- Run them with `nosplit-engine.sh` as the config's engine (the server adds `--layer-split` for a config with two
  GPUs; the wrapper drops it) - an experiment, not a setup option.

## What the cards cost to meet directly

`p2p_pingpong.hip`: card A writes a block into card B's VRAM and raises a flag there, B answers the same way, 10,000
times; one block of 256 threads, flags and buffers in uncached VRAM (cached flags are never seen to change: the first
version hung).

| block | one-way exchange |
|---|---|
| 4 KB | 2.8 us |
| 64 KB | 11.7 us |
| 256 KB | 40.3 us (~6.5 GB/s, the link) |

A decode window's hidden state is ~2.7 tokens x 2,560 floats, ~28 KB: ~6 us one way. Expert parallelism needs two
exchanges per layer (the activations to the other card, its weighted rows back): ~0.6 ms per window for 48 layers,
against the 8.6 ms the host-driven peer tier spends.

## A tensor split's bound

`tp_bound.py` on a `rocprofv3 --kernel-trace` of the layer split's decode (113 windows of the last request; the
profiler slows the windows, the kernels' own times stay close to the unprofiled ones):

| | per window |
|---|---|
| kernels, card A / card B | 1,218 / 1,217 (two thirds under 8 us, median 4.5 us) |
| kernel time, both cards in turn | 23.2 ms |
| a tensor split: each card runs all 2,435 kernels at half the work, down to a 4-8 us floor | 13.5-15.1 ms per card |
| its exchanges: 2 per layer, ~6-8 us each | ~0.7 ms |

So a tensor split could save ~7-9 ms of a ~28-30 ms window before the extra launch gaps (each card would run twice
as many kernels as now, ~2-3 us each: ~3 ms) and anything the host must coordinate: roughly +15-40% decode, an
estimate, not a measurement. It would take a new engine mode: every weight split in two (dense, experts, head, MTP),
partial outputs and an exchange after every attention and expert block, the KV cache and recurrent state split by
heads, the prompt path too.

## What would pay off most

| idea | estimate | work |
|---|---|---|
| the peer tier without the host at each layer: the main card plans the peer's share on the GPU, writes the activations and the plan into the peer's memory and raises a flag; a graph on the peer waits for it, computes, writes its rows back and raises a flag the main card waits on | the 8.6 ms of host per window -> ~0.6 ms of exchanges: ~22-24 ms windows, ~85-95 tok/s (vs 72.9) | days: a device-side plan for the peer, a peer graph per window, flags in uncached VRAM, the CPU's experts (the rest) unchanged |
| a tensor split | +15-40% | weeks |

Both are estimates from the measurements above; neither is built.

## Reproduce

```sh
B=bench/results/2026-10-05-split-decode-7900xtx; R=bench/results/2026-10-06-expert-parallel-7900xtx
python3 $B/bench.py --label split                                                   # the layer split
python3 $B/bench.py --label peer --exe $PWD/$R/nosplit-engine.sh --drop-arg=--remote-expert-opt \
    --arg=--peer-device --arg=1 --arg=--peer-reserve-mib --arg=3072
python3 $B/bench.py --label helper --exe $PWD/$R/nosplit-engine.sh --arg=--expert-cache-device1 --arg=11000
hipcc -O2 --offload-arch=gfx1100 $R/p2p_pingpong.hip -o p2p_pingpong && HIP_VISIBLE_DEVICES=1,0 ./p2p_pingpong 16384
```
