# Fewer kernels per token: 2x RX 7900 XTX (2026-10-06)

**Question:** a decode window launches ~2,400 small GPU kernels; does merging them make one conversation faster?

**Answer, measured:** a little. One merge kept: the CPU's expert rows, the GPU's and the combine in one kernel
(`combine_gather`), bit-exact, ~1% faster per window. Merging is worth it only for kernels on the decode window's
path from layer to layer; a merge elsewhere (the commit's) measured nothing. The ceiling with much more work is
estimated at ~5%.

Same PC, model and config as bench/results/2026-10-05-split-decode-7900xtx (2x RX 7900 XTX, ROCm 7.9.0, IQ3_XXS,
the layer split), engine from the `multi-agent-batch` branch.

## What a kernel costs

`graph_gap.hip`: a captured HIP graph of N tiny kernels, replayed: **2.86 us per kernel** (100-2,400 kernels, 1 or 40
blocks) - the floor of any kernel in a graph on this GPU.

`kinv.py`, `kpairs.py` and `klayer.py` on a `rocprofv3 --kernel-trace` of the split's decode (113 windows):

- 2,435 kernels per window on the two cards, 23.2 ms of kernel time; two thirds under 8 us (median 4.5 us).
- A recurrent (GDN) layer is 40 kernels, ~267 us of kernel time and ~3.6 us before each one (~145 us): about a third
  of a layer's GPU time is the space between kernels.
- The most frequent neighbours per window: the hyper-connection norm -> down -> up (102), the QSA indexer's appends
  (79 back to back), the CPU rows -> GPU rows -> combine (48 each), up -> quantize (52), norm -> rope (44),
  recurrent step -> quantize (36), projection -> quantize (31).

The existing opt-in switches (`STRATA_GR_SPLIT`, `STRATA_GR_V3`, `STRATA_FUSE_HEAD_GR`, `STRATA_GR_DOWN_MAX4`,
`STRATA_SEL_OVERLAP`) measured 66.8-69.1 against 68.7 tok/s: none worth turning on; `--pcie-frac 0` was 6% slower
(the PCIe group does work).

## Measured merges

Fresh start, `--adapt-every 0`, a story and a code prompt, greedy 512 tokens; the same tokens give the same windows,
so ms per window (`winms.py`, from `STRATA_DECODE_TIMING`) compares exactly.

| merge | kernels per window | same tokens | ms per window, on | off | decision |
|---|---|---|---|---|---|
| F2: the CPU rows, the GPU rows and the combine in one kernel | -97 | yes (and `tools/batch_test.py` IDENTICAL) | 34.02, 33.68, 34.10 | 34.53, 34.00 | kept: ~-0.34 ms (~1%) |
| F1: the QSA indexer's appends of a window and of a commit in one sequential launch | -79 | yes | 34.00, 34.06 | 34.04, 34.04 | dropped: no gain |

- **F2** (`native_moe_combine_gather_multi`): the doorbell path copied the CPU's rows from mapped memory into
  `parts` (zeros for the GPU's rows), added the GPU's rows, then combined. Now one kernel reads each expert row where
  it was computed (`0.0f + gpu_row` keeps the add's signed zero) and sums in the combine's order. Not used for the
  resident or device-planned paths or the helper's reduced rows. `STRATA_COMBINE_GATHER=0`: the three kernels.
- **F1:** most of the 79 appends are in the commit graph (all 6 window slots per attention layer, unkept ones
  return at once). Since the split's commits no longer wait, the commit runs while that card is idle anyway (the
  other stage, the draft), so the launches it saves were never on the path.

The text of one F2-off run differed from the others (466 windows instead of 463): with `--adapt-every 0` the runs
are reproducible in almost all runs today, not every one. Hashes from before the container was recreated differ
(the desktop card left 16 fewer expert slots).

## How much more is there

Only kernels between one layer's ring and the next count: ~1,900 per window. Merging ten tiny ones per layer (480 a
window) would save ~1.4 ms of ~28-30: ~5%, with each merge checked for the same tokens. The next candidates on that
path: the quantize after up / the recurrent step / a projection (~119 per window, ~1%), norm -> rope (44, ~0.4%),
conv -> alpha/beta (35, ~0.3%).

## Reproduce

```sh
B=bench/results/2026-10-05-split-decode-7900xtx; R=bench/results/2026-10-06-kernel-fusion-7900xtx
python3 $B/bench.py --label on --arg=--adapt-every --arg=0 --reps 1 --env STRATA_DECODE_TIMING=1
python3 $B/bench.py --label off --arg=--adapt-every --arg=0 --reps 1 --env STRATA_DECODE_TIMING=1 --env STRATA_COMBINE_GATHER=0
python3 $R/winms.py $B/data/logs on off
hipcc -O2 --offload-arch=gfx1100 $R/graph_gap.hip -o graph_gap && HIP_VISIBLE_DEVICES=1 ./graph_gap
```
