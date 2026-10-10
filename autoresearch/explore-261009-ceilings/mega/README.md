# Option B on AMD: fewer kernels per decode window (a persistent layer kernel) - the investigation, 09-10 Oct

The question: decode runs ~1,060 kernels per window on card 1 (~850 on card 2) and the gaps between them were
measured as large as the whole bandwidth floor (`../README.md`, section 3). What would merging them into a few
persistent kernels per layer return on *this* hardware (2x RX 7900 XTX, gfx1100, ROCm 7.9), what does the HIP path
allow, and is there a cheaper way to the same end? Everything below was measured on card 1 (HIP device 1) beside the
idle production server unless said otherwise; the programs are in this folder, their outputs in `*-dev1.txt`.

## 1. What a launch costs here, and what a device-side barrier costs (`launch_gap.cpp`)

| | per kernel / phase |
|---|---|
| empty kernel inside a hipGraph (96 blocks) | **4.19 us** |
| empty kernel as a plain stream launch | 3.35-4.44 us |
| atomic grid barrier inside one persistent kernel (96 blocks resident) | **0.37 us** |
| `cooperative_groups::grid_group::sync()` (hipLaunchCooperativeKernel works on gfx1100) | 0.58 us |

So a graph brings no launch benefit on ROCm 7.9 - the ~4 us is the command processor's dispatch cost per packet, paid
at every kernel boundary where the next kernel depends on the last. A barrier between phases of one kernel costs a
tenth of that.

## 2. Small reads: kernels against phases of one kernel (`phase_bw.cpp`, 384 MB streamed, no cache reuse)

| phase size | as graph kernels (192 blocks) | as phases of one persistent kernel | less |
|---|---|---|---|
| 2 MB (an expert blob, a 2560x512 bf16 projection) | 5.9-7.1 us, **294-355 GB/s** | 3.2-3.4 us, **615-650 GB/s** | 45-52% |
| 8 MB (a hyper-connection matrix) | 11.7-11.9 us, 705-718 GB/s | 9.7-9.8 us, 855-861 GB/s | 16-18% |
| 24 MB | 30.6 us, 822 GB/s | 28.1 us, 895 GB/s | 8% |
| 96 MB | 115.7 us, 870 GB/s | 110.4 us, 912 GB/s | 5% |

A 2 MB kernel runs at a third of the card's bandwidth (launch, ramp-up, tail); as a phase it runs at two thirds. The
decode window is made of exactly these sizes: 2.8 GB per window on card 1 in ~430 "big" kernels of 2-8 MB each and
~630 small ones.

HIP constraint found on the way: a persistent grid must be fully resident or its barrier deadlocks. On gfx1100 that is
**48 multiprocessors x 8 blocks of 256 threads = 384 blocks** for a simple kernel (fewer with more registers or LDS;
`hipOccupancyMaxActiveBlocksPerMultiprocessor` gives the number); 768 blocks hung the first run of this benchmark.
A persistent kernel therefore has to be written as a fixed grid of 96-384 blocks looping over work items, and every
kernel it absorbs (experts at 80 x cap blocks, projections at hundreds of blocks, attention chunks) has to be
re-expressed that way. On card 2 the drafter's chain runs on a second, high-priority stream beside the window: its
blocks would delay a persistent grid's residency (a stall, not a deadlock, since its kernels finish on their own), and
two persistent kernels on one card could deadlock each other - the grid should take at most half the resident limit.

## 3. Two cheaper mechanisms measured (`any_order.cpp`, `any_order_graph.cpp`, `fork_cost.cpp`)

**`hipExtAnyOrderLaunch`** (HIP's launch flag that drops the barrier packet between consecutive kernels of one stream)
on 2-MB kernels: plain launches 7.51 us each, any-order **3.76 us** - the same as the persistent phase. Empty kernels do
not move (4.44 -> 4.51 us): the dispatch itself still costs 4.4 us, but the next kernel's dispatch and ramp-up hide
under the current one's execution. Captured into a graph the flag keeps only part of it: 6.63 -> 5.49 us.

**Parallel graph branches** (fork / join on events inside the capture) keep all of it: two branches of 75 2-MB kernels
run at **3.90 us per kernel** against 6.63 us as one chain (-41%); with 96-block kernels 3.74. And a *per-layer*
fork/join (27 layers x [7 side kernels | 6 main kernels, join], `fork_cost.cpp`) costs nothing extra here: 1.87 ms
against 2.15 ms as one chain (192 blocks), 1.57 against 1.72 (96 blocks). This contradicts the engine's HIP default -
the shared-expert fork is off on HIP because on a Radeon AI PRO R9700 (gfx1201, ROCm 6.4.3) and an RX 6800 it cost
30% of decode (verify.cpp:132-148, #816) - but on gfx1100 + ROCm 7.9 a cross-stream dependency inside a graph is cheap.

So, on this box, two ready-made opt-ins fork independent work inside the window without a single new kernel:
`STRATA_SH_STREAM=1` (the shared expert's 7 kernels beside the router -> doorbell -> plan wait -> experts chain) and
`STRATA_DF_BRANCH=1` (the GDN mixer's a/b and z projections beside q/k/v and the conv; off by default after an NVIDIA
Linux hang with NVML queries, #905, never seen on HIP). Both leave the sums in the same order, so the text is the same.
Measured below (section 5).

## 4. The window's kernels on HIP (from `src/core/verify.cpp`, read for this; production settings)

Per layer, one stream, a pure chain (the pipeline's `always_publish` keeps the host doorbell; no hipBLAS anywhere in
decode - every projection is one of Strata's own kernels, so everything is mergeable in principle):

| part | kernels | of which big (MBs read) |
|---|---|---|
| hyper-connection read, attention half (norm -> down 6.5 MB -> up 6.5 MB, +q8 tail under QFUSE) | 3 | 2 |
| GDN mixer (qkv, conv, a/b, z, state step + norm, out) | 6 | 4 |
| QSA mixer (k, v, indexer k/q, q, norms + rope, KV append, indexer append, block scores, top-k, attention chunks, merge, gate, quantize, o) | 22 (24 with KV streaming) | ~8 |
| hyper-connection read, FFN half | 3 | 2 |
| router (bf16 GEMV 2.6 MB, top-10) + doorbell publish | 3 | 1 |
| shared expert (quantize, gate, up, swiglu+quantize, down, scalar gate, scale) | 7 | 3 |
| wait A -> plan copy -> VRAM experts (gate/up, swiglu, down) -> wait B -> fetch -> rebase -> PCIe pass (3) -> wait C -> combine | 13 | 3 (+3 rarely) |
| **GDN layer** | **35** | 14 |
| **QSA layer** | **51** | ~20 |

Card 1 (21 GDN + 6 QSA): ~1,041 layer kernels + ~20 (prologue, PLE, hand-off) ≈ 1,060 per window, plus a commit graph
of ~56 after every window on HIP. Card 2 (16 GDN + 5 QSA): ~830-850 + 44 - the ~938 measured on 07 Oct.

The grid-wide dependencies that a merge must respect (each a whole-row reduction = one barrier): the RMS norm's sum of
squares, each GEMV over the full input row, the top-k over 512 experts, the attention merge; single-block steps (top-k,
indexer append, doorbell publish with its system fence, KV resolve) serialize the grid; and three host spin waits per
layer (flags A, B, C on mapped memory, with `__threadfence_system`) must keep their exact semantics - one block spins,
the rest wait at the barrier.

## 5. What it would return (estimates from sections 1-2 and 4; A/B rows from measurements)

Per boundary removed, the saving is ~3.7 us for a small kernel (4.2 -> 0.4-0.6) and ~2-4 us for a 2-8 MB one (the
phase runs at twice the bandwidth). Over ~1,000 boundaries that is the whole 3-4 ms of gaps in card 1's 10.6 ms window:
**the ideal persistent window is ~7 ms per card (-30%)**; the pipeline then gives roughly +25-35% decode. A staged
version, each stage measurable on its own:

| stage | what | boundaries removed per window (card 1) | estimate | work |
|---|---|---|---|---|
| 0 | the two graph branches that exist (`STRATA_SH_STREAM=1`, `STRATA_DF_BRANCH=1`) | none; ~10 kernels per layer overlap the chain | **measured: 0.70-0.74x, dropped** | none |
| 1 | horizontal merges: the GDN's qkv ∥ z ∥ a/b in one launch, the QSA's five projections in one, the shared expert's gate ∥ up | ~75 | 2-3% | days |
| 2 | the FFN half as one persistent kernel per layer (router -> doorbell -> shared expert -> wait A -> experts -> wait B -> PCIe -> wait C -> combine): identical for both layer types | ~22 x 27 = 600 | **12-17%** (revised 10 Oct after measuring: +6-12%, section 8) | 2-3 weeks (the expert kernels as work loops over a fixed grid; the three host waits inside) |
| 3 | the hyper-connection triple + the GDN mixer as one kernel (3 barriers + 2) | ~10 x 21 = 210 | 5-7% | 1-2 weeks |
| 4 | the QSA mixer (attention chunks and merge inside a fixed grid) | ~25 x 6 = 150 | 3-4% | 2 weeks, the hardest |

Stage 2 alone is most of the gain; stages 1 and 3-4 are smaller and progressively harder. Risks: (a) each merged phase
must reproduce the current kernels' sums in the same order to stay bit-identical (or be checked by the quality suite);
(b) the fixed grid's efficiency on the expert kernels (80 x cap blocks today) is not measured; (c) the host-wait phases
inside a kernel need the same fences as the spin kernels (#697's RDNA double fence); (d) card 2's drafter stream and the
"parallel" slots' windows share the card with a kernel that wants it whole; (e) CUDA keeps its own path (the change is
HIP-only, under an env switch, default off until measured).

### Measured A/Bs (eight prompts, 512 tokens, alternating rounds, the production engine)

| branch | rounds (ratio to the default, prompts faster of 8) | mean tok/s |
|---|---|---|
| `STRATA_SH_STREAM=1` (the shared expert beside the router/doorbell/expert chain) | 0.697 (0/8), 0.742 (0/8), 0.705 (0/8) | 109.2 -> 77.7 |
| `STRATA_DF_BRANCH=1` (the GDN mixer's a/b and z beside q/k/v) | 0.735 (0/8), 0.734 (0/8) | 108.0 -> 79.2 |
| both | 0.790 (0/8), 0.519 (0/8) | 108.0 -> 71.5 |

**Any fork inside the window costs 26-30% of decode on this box**, the same fall upstream measured on gfx1201 (ROCm
6.4.3) and the RX 6800, although the stand-alone fork/join benchmark (`fork_cost.cpp`, 351 nodes) showed none.
`fork_cost2.cpp` repeats the fork at the window's scale (1,053 nodes, the window's mix of 1-block and 2-MB kernels):
the GPU side still gains (6.96 -> 5.69 ms per window; 4.97 -> 5.28 with 2-MB kernels only), but **`hipGraphLaunch` of
a graph with branches blocks the calling thread 1.4-1.6 ms per launch against 0.14-0.27 ms for a one-stream chain** -
HIP runs a multi-stream graph with the host involved. In the engine that thread is the one that answers the layers'
doorbells and drives the pipeline's chain, and the window's spin kernels wait for it; a window that needs the host
every layer cannot afford a launch that holds the host for 1.5 ms. That is the mechanism the toy could not show
(its host had nothing else to do). Whatever the mechanism, the
consequence for option B is firm: on HIP the window must stay one chain of kernels on one stream - overlap has to come
from inside a kernel (phases of a persistent kernel, or horizontal merges of independent GEMVs into one launch), not
from graph branches. The stage-0 row above is therefore closed (measured, worse), and stage 1 (horizontal merges) is the
first real step.

## 6. The verdict for AMD

- The gap is real and measured: ~4.2 us of dispatch at each of ~1,000 dependent kernel boundaries per window, plus
  2-8 MB kernels that run at a third to two thirds of bandwidth; one persistent kernel per layer half would take the
  window from ~10.6 to ~7 ms on card 1 (estimate from sections 1-2), roughly **+25-35% decode** at the end of all four
  stages, **+12-17% after stage 2 alone** (the FFN half: router -> doorbell -> shared expert -> expert passes -> combine,
  the same kernel for both layer types).
- The shortcuts are closed: graph branches cost 26-30% in the engine (sections 3 and 5), `hipExtAnyOrderLaunch` keeps
  only part of its gain inside a graph, and a graph brings no launch benefit over plain launches on ROCm 7.9. On HIP
  the window stays one chain on one stream; overlap must come from inside a kernel.
- What the persistent kernel has to be on gfx1100: a fixed grid of at most half the resident limit (192 of 384 blocks of
  256 threads, so card 2's drafter stream and a second window can still run), every absorbed kernel re-expressed as a
  work loop over that grid, the three host flag waits per layer as spin phases of one block with the same system
  fences, the row reductions (RMS norm, GEMV, top-k, attention merge) as grid barriers at 0.4-0.6 us each.
- Order of work: stage 2 first (most of the gain, no attention code, identical for GDN and QSA layers), measured on
  its own behind a HIP-only env switch with the exact protocol and the eight-prompt rounds; stage 1 (horizontal merges
  of independent GEMVs, ~3%) and stages 3-4 after, if stage 2 pays what the estimate says. CUDA untouched.
- The risk that decides it is (b) in section 5: whether the expert kernels (today 80 x cap blocks of their own shape)
  stream VRAM as well from a fixed 192-block grid. That can be measured first, in a day, with a stand-alone copy of
  `native_gu_*` / `native_down_multi_kernel` over a fixed grid against the originals - the go/no-go for the weeks.

## 7. The go/no-go probe: the expert kernels from a fixed grid (10 Oct, `expert_fixed_grid-dev1.txt`)

`build-hip/native_expert_bench` (the engine's own grouped expert kernels on real rows of the GGUF, card 1) with a new
switch `NATIVE_EXPERT_BENCH_GY=N`: the launches get N block rows that stride over the experts (the launcher's existing
`grid_groups`, which the window's PCIe pass already uses) instead of one block row per expert. A block row is 80 blocks
for gate/up and 80 for down, so N = 2 / 4 is a fixed grid of 160 / 320 blocks - what a persistent kernel would run.
A decode step's layer has ~18 experts with one row each (two rows, ten experts each, mostly distinct); also 9 x 2.

| layer, experts x rows | one block row per expert (today) | fixed 320 blocks (N 4) | fixed 160 blocks (N 2) | fixed 80 blocks (N 1) |
|---|---|---|---|---|
| 3, 18 x 1 | 56.3 us | 55.7 us (-1%) | 64.3 us (+14%) | 75.1 us (+33%) |
| 10, 18 x 1 | 57.5 us | 57.8 us (0%) | 65.6 us (+14%) | 74.8 us (+30%) |
| 3, 9 x 2 | 42.6 us | 41.0 us (-4%) | 45.3 us (+6%) | 52.0 us (+22%) |
| 10, 9 x 2 | 44.8 us | 42.3 us (-6%) | 46.5 us (+4%) | 51.7 us (+15%) |

Bitwise equal in every case (the group stride does not change any sum). The times include each kernel's launch
(gate/up, swiglu, down), so they are the stand-alone kernels, not phases.

**GO.** A 320-block grid - inside gfx1100's 384 resident blocks - runs the experts as fast as today's launches. At
160 blocks (half the resident limit, the margin for card 2's drafter stream) they are 4-14% slower: ~7 us per layer,
~0.2 ms per window on card 1, against the ~3-4 ms of launch gaps a persistent window would remove. So the grid size
is not the obstacle. Card 1 has no drafter stream and can take 320; card 2 can start at 192-256 and be measured.

Seen on the way: even launched alone the expert kernels read at 300-490 GB/s, a third to half of the card's 917 -
18 experts of ~1.3 MB each are too little work per kernel to stream at full speed. Phases of one kernel that start the
next expert's reads while the last ones finish are where the rest of stage 2's gain would come from (section 2: 2-MB
phases at 615-650 GB/s against 294-355 as kernels).

## 8. Step 1 of the build: the FFN half measured on real weights (10 Oct)

**A kernel trace of the production decode** (`rocprofv3 --kernel-trace`, story + code, 256 tokens; the trace slows
decode tenfold, so only the kernels' own durations are used): each layer's FFN half is exactly **23 kernels** on both
cards (router GEMV, top-10, doorbell, the shared expert's 7, wait A, plan copy, gate/up, swiglu, down, wait B, fetch,
rebase, the PCIe pass's 3, wait C, combine), ~111-124 us of kernel time per layer.

**Found on the way, fixed and installed:** `fetch_blobs_kernel` (the PCIe share's copy into staging) took 12-15 us in
every layer although it nearly never has anything to copy (card 2: never in the trace). `empty_grid.cpp`: an empty
launch of **384 x 256 blocks - exactly 48 CUs x 8 - costs 14.0-15.8 us; 352 blocks or fewer 2.7-3.0 us**, on both
cards; a real copy of one 2.3 MB blob from mapped host memory runs at 6.9 GB/s with 16 to 384 blocks alike. No other
kernel of the window launches 384 blocks. The copy now launches 96 blocks on HIP (`verify_kernels.cu`; CUDA keeps 384).
Measured against the installed engine: exact protocol the same text (hashes 928818aa87 / 449d3d4c28 both), story
88.7 -> 91.5, code 108.4 -> 113.3 tok/s; eight prompts in three alternating rounds 1.100, 1.014, 1.062 (19 of 24 prompts
faster; means 106.5 -> 112.5 tok/s). In production since 10 Oct (`engine/strata-before-fetch96` is the old one).

**The harness** (`src/kernels/ffn_half_bench.cpp`, built as `build-hip/ffn_half_bench`): the FFN half of 8 layers
(1, 2, 4, 5, 6, 8, 9, 10) on their real GGUF weights in one graph, 2 rows, every routed expert in VRAM, the plan
written into mapped memory as the pool writes it, the host flags raised beforehand. Card 1, idle:

| | per layer |
|---|---|
| the chain as it runs today (23 kernels, untraced) | **150 us** |
| the sum of the 23 kernels' own durations (the same harness under the trace) | 114 us |
| so the gaps between the kernels | **~36 us** (1.6 us per boundary) |

Inside a graph the next kernel's dispatch overlaps the running one, so a boundary costs ~1.6 us here, not the 4.2 us
an empty kernel costs (section 1). That halves the estimate of section 5 for the gaps alone: ~36 us x 27 layers =
~1.0 ms per window on card 1 (~9%), ~0.75 ms on card 2. What a persistent FFN half could gain beyond that is the
kernels running faster as phases: per layer it reads ~31 MB (experts ~26, router 2.6, shared expert ~2.7), 35-48 us at
650-900 GB/s against the 114 us the kernels take; the 2-MB-phase measurement (section 2) says phases get about twice
the bandwidth of kernels, so 114 -> ~60-70 us is plausible but not shown. **Revised estimate for stage 2: per layer
150 -> ~85-110 us, decode roughly +6-12%** (was +12-17%), the upper half only if the expert phases stream as the
micro-benchmark did.

## Files

`launch_gap.cpp`, `phase_bw.cpp`, `any_order.cpp`, `any_order_graph.cpp`, `fork_cost.cpp` and their `*-dev1.txt`
outputs, `expert_fixed_grid-dev1.txt` (section 7; `src/kernels/native_expert_bench.cpp`'s new `NATIVE_EXPERT_BENCH_GY`) (`fork_cost2.cpp` with the host-blocking measurement, run 10 Oct morning); the A/Bs ran with `~/strata-tools/d7.sh`, `d8.sh` (copies here), results `D7-*`, `D8-*` in
`../data/branches.jsonl`.
