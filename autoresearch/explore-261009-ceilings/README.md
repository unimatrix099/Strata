# Ceilings: how fast this PC could decode and read prompts, and where the time goes (2026-10-09)

The PC: 2x RX 7900 XTX (gfx1100, 96 CUs, 24 GB each; one drives the desktop), Ryzen 9 5950X, 128 GB DDR4, both
cards on PCIe 3.0 x8; the fork's production `main` with IQ3_XXS, 128K context, the tuned pipeline. Measured today:
decode 97-108 tok/s (17 ms per window, 1.85 tokens per window), prompt reading 1,100-1,460 tok/s.

## 1. The hardware ceilings (measured, `gpu_bw.cpp`, `cpu_bw.cpp`, `gemm_tflops.cpp`)

| path | measured | spec |
|---|---|---|
| VRAM read, a streaming kernel, each card | **917 GB/s** (903-924) | 960 GB/s (GDDR6 20 Gbps x 384 bit) |
| VRAM copy (read + write), each card | 790 GB/s | |
| pinned RAM -> card, card -> RAM (PCIe) | **7.08 / 7.15 GB/s** | 7.9 GB/s (PCIe 3.0 x8; sysfs shows the switch's "16 GT/s x16" uplink, not the cards') |
| card -> card (`hipMemcpy` peer) | 7.01 GB/s | over the same link |
| CPU RAM read (AVX2) | 30 GB/s one thread, **45 GB/s** two or more | ~51 GB/s (DDR4-3200, 2 channels) |
| BF16 GEMM, `hipBLAS` (`hipblasGemmEx`), M 512-8192 | 12-15 TFLOPS | 123 TFLOPS FP16/BF16 WMMA; the engine's own GEMM path reaches ~31 TFLOPS on the expert GEMMs below (gemm.cu's probes: 29-35) |

## 2. What one token costs (from the GGUF: `roofline.txt`)

48 layers (36 GDN, 12 QSA), n_embd 2560, 512 experts of n_ff 640, 10 used + 1 shared, 4 hyper-connection streams
(rank 320, **bf16: 26.5 MB per layer, 1.27 GB per window - 37% of all dense bytes**), head 2560 x 248,320 (Q5_K,
437 MB), the per-layer token embedding 28.8 GB in host RAM (a row per token and layer).

Bytes read per decode window (the weights are read once per window, whatever its row count):

| | dense | experts (18.6 entries per layer at T 1.55) | per window | at 917 GB/s |
|---|---|---|---|---|
| card 1 (layers 0-26) | 1,925 MB | 878 MB | 2,803 MB | **3.06 ms** |
| card 2 (layers 27-47 + head) | 1,934 MB (437 the head) | 681 MB | 2,615 MB | **2.85 ms** |

Prompt arithmetic: 13.2 GFLOP per token (experts 4.7, dense 8.5), the head once.

## 3. Decode: the roofline against the measurement

| | ms per window | tok/s at 1.85 tokens per window |
|---|---|---|
| bandwidth floor, the cards taking turns | 5.9 | 314 |
| bandwidth floor, the cards perfectly overlapped | 3.1 | 606 |
| **measured (production)** | **17** | **105** |

Decode runs at **17% of the overlapped roofline, 33% of the serial one**. Each card's half is at ~29% of its bandwidth:
card 1 10.6 ms, card 2 9.8 ms (`data/decode-profile.txt`, `STRATA_VERIFY_PROFILE=1`, exact protocol, T 1.55; the draft
chain's ~4 ms runs beside card 2), and the pipeline overlaps them only on the 49% of windows whose guess was right
(a kept speculative window ~11 ms, a fresh one ~22 ms).

Where card 1's 10.6 ms goes (the parts overlap, so they sum above the total): GDN projections and recurrence 2.2,
hyper-connection read + router 1.8, the hyper-connection's own norm / down / up 1.3, VRAM-hit experts 1.5, shared
expert + quantize 0.9, host waits (the plan, the doorbells, the CPU rows) 1.15, the QSA layers' attention path ~1.2,
PCIe group 0.3, combine 0.24. None of it is near its bandwidth floor: the 1.9 GB of dense weights alone would take
2.1 ms, the 0.9 GB of experts 1.0 ms. The kernel trace of 07 Oct explains the rest: **~938 kernels per window per
card, ~4 us of gap at each boundary = 3-4 ms, as much as the whole bandwidth floor**, and kernels of 1-2 MB each
(an expert, a projection) that never reach streaming bandwidth.

So the theoretical maximum for this engine's structure is not the 600 tok/s of the bandwidth roofline but roughly
**launch gaps + bandwidth ≈ 6-7 ms per card per window ≈ 250-300 tok/s overlapped**; the measured 105 is the
pipeline's misses (half the windows run serially) and the small-kernel inefficiency on top.

## 4. Prompt reading: the roofline against the measurement

| | tok/s |
|---|---|
| compute floor at the 123 TFLOPS spec (card 1's 27 layers, the cards overlapped) | 16,600 |
| compute floor at the practical ~31 TFLOPS the engine's GEMMs reach | **4,200** |
| PCIe floor: the 12% of experts not resident streamed per 8192-token chunk (5.1 GB at 7.1 GB/s) | 11,300 |
| **measured, 32K prompt, end to end** | **1,100-1,460** (first token after 21-27 s) |
| measured, card 1's GPU timeline alone (`STRATA_PREFILL_TIMING=1`) | 1,880 (16.3 s for 30.7K tokens) |
| measured, the steady chunk cadence (8192 tokens every ~4 s once both cards are busy) | ~2,050 |

Prompt reading is at **a third of the practical compute ceiling**. Card 1's 16.3 s on a 32K prompt
(`data/pf-base/prefill-timing.txt`):

| phase | s | share | note |
|---|---|---|---|
| attention (6 QSA layers, the indexer's top-2048 blocks) | 2.6 | 16% | |
| expert GEMMs (gate/up 1.8 + down 0.9) | 2.7 | 17% | 84 TFLOP: **31 TFLOPS**, the practical ceiling |
| dequantizing the experts for the GEMM | 2.1-3.0 | 13-18% | gone on the fused path (below) |
| host grouping (the CPU sorts the rows by expert) | 1.4-2.5 | 9-14% | CPU time the GPU waits for |
| combine (scatter-add of the expert rows) | 1.4 | 8% | its bandwidth floor is ~0.1 s |
| embed + steps | 0.6-2.4 | 4-15% | varies between identical runs (the host's PLE lookups beside it: 2.3 s of random reads in the 28.8 GB table) |
| GDN (projections, recurrence, conv) | 1.9 | 12% | |
| hyper-connection read, QSA projections, select, PLE, KV | 1.7 | 10% | |

Card 2 has no timeline on HIP (the peer line is CUDA-only), so what bounds the chunk cadence when card 1 gets faster
is not instrumented: with 16K chunks card 1's time fell 20% (13.1 s) and the end-to-end speed moved 1.6%.

## 5. Measured today: switches on the prompt path (32K prompt, the installed config)

| setting | prompt tok/s | first token after | card 1 GPU time |
|---|---|---|---|
| the installed config (two runs) | 1,438 / 1,395 | 21.4 / 22.0 s | 16.3 / 17.4 s |
| `STRATA_PF_FUSED=1` (the experts on the matrix cores, no dequant pass) | **2,057 (+45%)** | **14.9 s** | 10.9 s |
| `STRATA_PF_GEMM=1` (the engine's 128x256 WMMA GEMM for the projections) | 1,458 (+1.5%) | 21.1 s | 16.2 s |
| `--prefill 16384` (16K chunks) | 1,461 (+1.6%) | 21.0 s | 13.1 s |
| `STRATA_PREFILL_HELP=1` (the idle card helps one-chunk prompts), a 4.6K prompt | 1,248 against 1,251 | | the helper is off above ~3.3K tokens |

`STRATA_PF_FUSED=1` is upstream's own fused prompt path, opt-in because it **rounds differently** (the maintainers gate
it with a KL check on gfx1151; docs/STRIX_HALO.md). Its 32K summary was correct and coherent, with other wording than
the default's. Two identical default runs also wrote different summaries (the installed config is not run-to-run
exact: the adaptive tier and the PCIe share), so no exactness was established for any variant here; the fork's exact
protocol (`STRATA_IQ_MT_MIN=1 --pcie-frac 0 --adapt-every 0`) would settle `STRATA_PF_GEMM` and the chunk size.

## 6. What could close the gaps (ranked)

Decode (105 tok/s; the structural ceiling ~250-300, the bandwidth roofline 600):

1. **More tokens per window.** A window's weights are read once; an extra row costs card 1 only 1.1-1.3 ms of 10.6.
   Drafts accepted 77%, 1.85 tokens per window: a drafter that got 2.5 would be +35% at the same window time, and it
   would also raise the pipeline's right guesses (49%) - the single largest lever, and the one set aside (a new draft
   model, e.g. a self-distilled MTP head: FastMTP's 2nd / 3rd draft acceptance 11 -> 56%, 2 -> 36%).
2. **Fewer kernels per window** (938 per card): the launch gaps equal the bandwidth floor. Merging a layer's small
   kernels (the quantize, norm, doorbell, copy kernels) was measured at ~1% each on 07 Oct; a layer-level persistent
   kernel (megakernel) is the real version: +20-30% possible, weeks of work in the kernels.
3. **The hyper-connection weights in Q8_0 instead of bf16**: 1.27 GB of the 3.4 GB read per window are the four bf16
   hyper-connection matrices per layer; Q8_0 halves them (`STRATA_HC_Q8=1` exists for GGUFs that ship Q8_0 copies;
   ours has bf16, so a conversion at load would be needed). ~-0.7 ms per window if those kernels were at bandwidth
   (they are not: 3.1 ms for 1.27 GB); rounds differently - opt-in. Perhaps +5%.
4. **The host round trips** (1.15 ms per window per card): the device-planned layers (upstream's E-6) are off under
   the pipeline's `always_publish`; a device-side plan for the all-resident case would remove most of it: ~+6%.
5. The draft chain (4 ms, 312 kernels) sits on the miss path (51% of windows): the same launch-gap problem; a merged
   chain step would shorten every miss by ~2 ms: ~+5%.

Prompt reading (1,400 tok/s; the practical ceiling 4,200):

1. **`STRATA_PF_FUSED=1`: +45% measured today.** Not bit-identical; to adopt it in production, run the long-context
   quality suite with it on (needles, facts, the 96K conversation) and read a few answers. The easiest large gain on
   this PC.
2. **Host grouping on the GPU or overlapped** with the previous layer's GEMM (9-14%): the CPU sorts 8192 x 10 rows
   per layer while the GPU waits.
3. **The combine kernel** (8%): a scatter-add at ~1/10 of bandwidth; exact to fix.
4. **The host's PLE lookups** (2.3 s per 32K, 7%): 48 random reads per token in a 28.8 GB table; prefetching them for
   the next chunk while the GPU runs this one.
5. **Attention at long prompts** (16-23%): upstream's `STRATA_HIP_WMMA=1` / `STRATA_PA_FAST=1` (the prompt attention
   on matrix cores; round differently) - to measure.
6. **The pipeline's fill**: the first chunk runs card 1 then card 2 with nothing overlapped (~4 of 21 s at 32K, a
   larger share on 8-16K prompts): a smaller first chunk, or `STRATA_PREFILL_HELP` extended past one-chunk prompts.
7. Card 2's prompt timeline on HIP, so the cadence's bound is known.

## Files

`gpu_bw.cpp`, `cpu_bw.cpp`, `gemm_tflops.cpp` (the micro-benchmarks and their outputs `*.txt`), `roofline.txt` (the
cost model), `data/decode-profile.txt` (the per-stage decode profile), `data/pf-*/` (each prompt variant's timing, speed
and answer), `data/results.jsonl` (the helper test), `pfvar.sh` / `vprof.sh` (how they ran).
