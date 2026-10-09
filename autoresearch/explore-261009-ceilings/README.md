# Ceilings: how fast this PC could decode and read prompts, and where the time goes (2026-10-09)

The PC: 2x RX 7900 XTX (gfx1100, 96 CUs, 24 GB each; one drives the desktop), Ryzen 9 5950X, 128 GB DDR4, both
cards on PCIe 3.0 x8; the fork's production `main` with IQ3_XXS, 128K context, the tuned pipeline. Measured today:
decode 97-108 tok/s (17 ms per window, 1.85 tokens per window), prompt reading 1,100-1,460 tok/s.

## What the loop kept (09 Oct, short tests; each adopted after a quality check)

| change | prompt reading before -> after | where it pays |
|---|---|---|
| `STRATA_PF_FUSED=1` (upstream's fused expert kernels on the prompt path) | 32K 1,438 -> 2,057 tok/s; 118K 1,119 -> 2,787 | every prompt from 1,024 tokens |
| `STRATA_HIP_WMMA=1` (the prompt attention on the matrix cores) | 16K 1,685 -> 2,025; 32K -> 2,654-2,726; 118K -> 3,368 | long prompts (attention's share 24% -> 6-10%) |
| `STRATA_PREFILL_CPU_SHARE=1` (the CPU computes a short chunk's non-resident experts instead of streaming them) | 1K 500-538 -> 583-611; 2K 845-852 -> 864-910 | agent turns (reads under ~3K tokens) |
| `--ple-io ram` (the 28.8 GB per-layer embedding table in RAM instead of SSD reads; exact) | 4K 1,130 -> 1,384-1,455 (+22-29%); 32K 2,831 -> 3,093-3,119 (+9-10%); card 1's PLE blocking 1.8 s -> 0.08 s per 32K | every prompt; a 15 s table load on a cold start (0.1 s warm) |
| `STRATA_PA_FAST=1` (the prompt attention with single FP16 q and p) | 16K 2,025 -> 2,097; 118K 3,368 -> 3,434 | long prompts, +2-3.5% (step 13's check: needles 6/6 at 32K/128K, the facts at 118K, a 2K-token answer after 118K coherent) |

In all: a 1K prompt's first token 2.0 -> 1.7 s, 32K 21-27 -> ~12 s, 118K 101-108 -> 36 s; decode unchanged
(the loop found no cheap decode lever: see section 6). None of the three is bit-identical with the default path; the
quality suite (section 7 of the long-context README) passed with the first, a shorter check with the second and third.
Tried and dropped: device-planned layers under the pipeline, the idle card's prompt helper (never engages here),
`STRATA_HC_Q8` (inert on this GGUF), `STRATA_PF_GEMM`, `STRATA_PF_PAD`, `STRATA_HC_UPMIX`, `STRATA_SELECT_WMMA`,
`STRATA_PREFILL_STREAM_MIN=128`, 16K chunks (-13%), a smaller reserve on the desktop card, a later layer split (prompts
+5.6%, decode -1-7%). Decode at 118K with the attention cache resident to 64K instead of 32K (`--kv-resident 65536`, step 13): 95.2 against
95.0 tok/s on a 1,500-token answer - nothing (the earlier 73-78 at 121K was a 110-145-token answer's pipeline start).

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
is not instrumented: with 16K chunks card 1's time fell 20% (13.1 s) and the end-to-end speed moved 1.6%. The likely
bound (from the engine's start lines): card 2 holds 8,398 of its 10,752 expert pairs (78%) against card 1's 13,164 of
13,824 (95%) - the desktop card's 3 GB reserve, the head and the draft layer take its room - so per 8192-token chunk
it streams ~22% of 21 layers' experts, ~4 GB over PCIe (~0.6 s), against card 1's ~1.2 GB (~0.17 s). Card 1's
"waiting for each chunk" (1.1-1.5 s per 32K prompt) is that difference; a later layer split (section 5) shortens it.

**With the production path (fused + WMMA, `data/s7-base`, `data/s9-split27`):** card 1's whole-prompt timeline 8.5-8.6
s for a 32K prompt (2.15 s per chunk: expert GEMMs 25%, combine 16%, GDN ~20%, attention 10%, hyper-connection read
8%), card 2 1.75-1.79 s of GPU time per chunk (expert GEMMs 23%, **waiting for streamed experts 14%**, attention 14%,
GDN 20%) - each stage prints its own timing lines, the "8192 tokens" ones are card 2's. The server's progress shows
the first chunk done after 5 s (the two stages one after the other) and then **one chunk every 2.0 s (~4,000 tok/s in
the steady state)**; a 32K prompt's 11.6 s are the serial first chunk, three more at 2 s, and card 2's last. Card 1
still blocks 1.2-1.7 s per 32K prompt in the PLE row lookups (`ple_land`, the host line's "PLE"), ~0.3-0.4 s of each
chunk: the exact fix is to gather them further ahead or in parallel (the next code item) - or no code at all: the PLE reader
reads the 28.8 GB table from the SSD with one I/O thread (`src/ngram/ple_reader.cpp`), and the engine already has
`--ple-io ram` (the table mapped and locked in RAM; ~70 GB were free in every run here). Measured (step 14, `data/s14-*`): `--ple-io ram` 4K 1,130 -> 1,455 / 1,384 tok/s, 32K 2,831 -> 3,119 / 3,093, the PLE
blocking 1,811 -> 77 / 74 ms per 32K; decode unchanged; the same rows (exact). The engine loads the table in 15 s on a
cold start (0.1 s warm, from the page cache) and says "mlock failed (Cannot allocate memory; raise ulimit -l)" in this
sandbox: the pages stay faulted in but could be reclaimed under memory pressure (the Docker compose file sets memlock
unlimited; on a bare host raise `ulimit -l`). In the production config since 09 Oct.

**A kernel trace of a 4K prompt on the production path** (step 15, `rocprofv3`, `data/s15-prof/`): in the prompt's
2.7 s window each card runs kernels for only 0.75-0.80 s (27-29% busy). The kernels are not the problem - the
hipBLASLt GEMMs 0.13-0.16 s, the fused expert kernels ~0.3 s, the GDN recurrence 0.06, the WMMA attention 0.05 per
card; the time is the two stages running one after the other on a one-chunk prompt (each card idle while the other
works) and the waits for streamed experts. Hence step 16: smaller chunks, so a 4K prompt pipelines across the cards - measured (`data/s16-*`, with `--ple-io ram`
on): chunks of 8192 / 4096 / 2048 read a 4K prompt at 1,474 / 1,365 / 1,339 tok/s and a 32K one at 3,032 / 2,713 /
1,797: **worse at every size**, since each chunk streams card 2's non-resident experts again (~0.6 s per chunk). The
default 8192 stays. Short prompts are left with the serial two-stage structure as their bound; a one-chunk prompt
would need the stages to split the chunk between them (a design change), or more of card 2's experts resident.

## 5. Measured today: switches on the prompt path (32K prompt, the installed config)

| setting | prompt tok/s | first token after | card 1 GPU time |
|---|---|---|---|
| the installed config (two runs) | 1,438 / 1,395 | 21.4 / 22.0 s | 16.3 / 17.4 s |
| `STRATA_PF_FUSED=1` (the experts on the matrix cores, no dequant pass) | **2,057 (+45%)** | **14.9 s** | 10.9 s |
| `STRATA_PF_GEMM=1` (the engine's 128x256 WMMA GEMM for the projections) | 1,458 (+1.5%) | 21.1 s | 16.2 s |
| `--prefill 16384` (16K chunks) | 1,461 (+1.6%) | 21.0 s | 13.1 s |
| `STRATA_PREFILL_HELP=1` (the idle card helps one-chunk prompts), a 4.6K prompt | 1,248 against 1,251 | | the helper is off above ~3.3K tokens |

On top of the fused path, 16K prompt (short tests, one run each; `data/s2-*`):

| setting | prompt tok/s | first token after | attention's share of card 1's time |
|---|---|---|---|
| `STRATA_PF_FUSED=1` | 1,685 | 9.7 s | 23.6% |
| + `STRATA_HIP_WMMA=1` (the prompt attention on the matrix cores; the engine confirms it on gfx1100) | **2,025 (+20%)** | 8.1 s | 9.9% |
| + `STRATA_PA_FAST=1` (single FP16 q and p) | 1,734 | 9.4 s | 23.5% |
| + both | **2,097 (+24%)** | 7.8 s | 5.6% |

Both round differently from the default attention (docs/AMD_HIP.md: not bitwise). The WMMA attention passed a shorter
quality check (needles 6/6 at 32K/128K, facts 2/2, the 96K conversation, a 2,000-token answer after 118K) and is in
production since 09 Oct; `STRATA_SELECT_WMMA=1` on top: 2,031 tok/s, nothing. `STRATA_PA_FAST` waits for its check. Decode with `STRATA_HC_Q8=1`: 93.75 against 93.85 tok/s, and the engine reports
"0.00 GiB of Q8_0 hyper-connection projections": inert on this GGUF (its projections are bf16), as the code reads.

More on the fused base (short tests, one run each; `data/s3-*`): `STRATA_HC_UPMIX=1` 1,739 tok/s at 16K (+3%, noise),
`STRATA_PF_PAD=1` 1,695 (nothing); at 32K the fused base 2,090, 16K chunks 1,812 (**-13%**: fewer, longer chunks
overlap less), `--layer-split 28` 2,164 (+3.5%), `--layer-split 30` 2,207 (+5.6%; card 1's wait for card 2 per chunk
1.5 -> 0.9 s: on the prompt path card 2 is the slower stage) - but a later split cost decode 1-7% per window on 07 Oct,
so 26 stays. Short prompts: 1K tokens read at 488 tok/s (2.0 s to the first token), 2K 821, 4K 1,107 -
`STRATA_PREFILL_STREAM_MIN=128` changes nothing there. Where a 1.8K prompt's 2.2 s go (`data/s3-short-base`):
card 1's GPU timeline 0.9 s, of which **"wait copy" 0.42 s (46%)** - the experts not resident on card 1 streamed over
PCIe for a chunk too short to hide them - then card 1 waits 0.87 s for card 2 (the stages run one after the other
on a one-chunk prompt; card 2 streams ~22% of its experts, ~4 GB, ~0.6 s). So short prompts are PCIe-bound: more
resident experts on card 2 (step 7) and the idle card's help are the levers. `STRATA_PREFILL_HELP=1` (step 6, `data/s6-*`):
500 / 850 / 1,230 tok/s at 1K / 2K / 4K with and without it, and no helper line in the log - upstream documents it as
unavailable with the fused prompt kernels, which the config has on; step 8 measured it with the fused path off (`data/s8-*`): 501 / 727 / 1,026 tok/s without, 497 / 710 / 1,027 with,
and still no helper line - it does not engage on this setup (perhaps `--remote-expert-opt`); dropped. The same runs
show the fused path's own gain on short prompts: 2K 727 -> 851 tok/s, 4K 1,026 -> 1,231 (+17-20%); at 1K both paths
are the plain MMQ one (the fused kernels start at 1,024 tokens).

Step 10 (`data/s10-*`, 1K / 2K prompts on the production path): **`STRATA_PREFILL_CPU_SHARE=1`** (upstream's CPU share
of a small chunk's experts, on by default only on one-GPU CUDA) 1K 499 -> 581 tok/s (+16%, the first token after 1.7 s
instead of 2.0), 2K 847 -> 864; card 1's GPU timeline 886 -> 622 ms with "wait copy" 468 -> 7 ms - the CPU computes
the experts not resident on the card instead of the card waiting for them over PCIe. `STRATA_PREFILL_STREAM_MIN=128`
(the fused kernels from 128 tokens): 497 / 792, nothing or worse. Step 11 (`data/s11-*`): a repeat 1K 538 -> 611 tok/s (+13%), 2K 852 -> 910 (+7%); its answers on short prompts right
(needles at 1K 3 of 3, the facts at 1K, `smoke.py` 7 of 7, no engine error). Step 12 (`data/s12-*`, 1K / 2K / 4K): the base 502 / 845 / 1,227 tok/s, `auto` 564 / 857 / 1,202, `0.5` 541 / 830 /
1,198, **`1` 583 / 864 / 1,199** - the fixed full share is best at 1K-2K and neutral at 4K; in the production config
since 09 Oct.

With the fused path and the WMMA attention (the production config since 09 Oct), a 32K prompt reads at 2,654 tok/s
(step 7's base; the default path 1,438). More cache on the desktop card (`--vram-reserve-later-mib 2048` / `1536`,
`data/s7-*`): its cache 8,398 -> 9,032 / 9,301 slots, but the automatic split then moved layer 26 onto it, and the
prompt read slower (2,475 / 2,523 tok/s, card 1's wait per chunk 1.5 -> 1.7-1.8 s); the GTT stayed at idle (32 / 106
MB) even at 1536. Step 9 repeated it with the split pinned at 27 (`data/s9-*`): card 2's cache 8,398 -> 8,933 / 9,201 slots (88 -> 90
/ 91% resident), 32K prompts 2,726 / 2,647 / 2,699 tok/s (noise), 4K 1,164 / 1,226 / 1,257 (+5-8%, single runs),
card 1's wait per chunk still ~1.7 s. So card 2's bound on the prompt path is not its expert residency; what it is
(its draft-layer pass per chunk, 0.2-0.5 s in the host line, its hand-offs, its compute) needs card 2's own prompt
timeline, which the HIP build does not record (the peer timeline is CUDA-only) - a code item for later. The reserve
stays 3072 (decode is unchanged by it and the desktop keeps its room).

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

Measured (step 5, `data/results.jsonl` S5-*): **device-planned layers under the pipeline** - an opt-in that let the
E-6 device plan run with `set_always_publish` (the tier uploading the device's residency table before its fence):
exact protocol 18.25 / 19.07 ms per window without, 18.56 / 19.62 with; the installed config 17.10 against 17.25.
The CPU-flag wait went, the A / B waits stayed (the groups are rarely all resident at 98-99% hits per expert, and the
doorbell ring stays under `always_publish`). No gain: the code was not kept.

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

## Tests still to run (the long ones, later)

Started 09 Oct, evening, unattended (`~/strata-tools/long1.sh`; results land in `~/strata-tools/longctx/final-full`,
`final-par4`, `final-cc` and `~/strata-tools/data/results.jsonl` as `FIN-*`): the full long-context suite of the final
config, eight-prompt decode pairs against the config before today's prompt switches, 4 x 64K at once, the conversation
cache. Not yet read.

The loop now uses short tests (16K prompts, 256-token decodes, one run each) to find candidates; before any of them
goes into the production config it needs the long checks:

- the full long-context quality suite (`longctx.py`, ~25 min) with the candidate on - needles 8K-128K, facts, the
  ~3K-token answers after 32K-122K, 16K generations, the 96K conversation, and a read of any flagged answer;
- the exact-protocol runs (`STRATA_IQ_MT_MIN=1 --pcie-frac 0 --adapt-every 0`, story + code x2, ~6 min) to say whether
  it is bit-identical to the default path - **with `STRATA_PF_FUSED=0 STRATA_HIP_WMMA=0` in the run's environment**
  since 09 Oct: with the production config's fused prompt path and matrix-core attention two identical starts wrote
  different texts even under the exact protocol (step 5's base runs), so a comparison needs both off - `STRATA_PF_GEMM`, `--prefill 16384` and `STRATA_HC_Q8` have not had it;
- the eight-prompt decode speed pairs (`bench_many.py`, alternating with the production engine, 3 pairs, ~30 min) for
  any decode change claiming under 5%;
- several conversations at once (`"parallel"` 4 at 64K) and the conversation cache switches, for a change on the
  prompt path (the prompt path runs while slots decode);
- `smoke.py` (7 checks) and `tools/batch_test.py` after any engine rebuild.

Done at full length so far: `STRATA_PF_FUSED=1` - the long-context suite (section 7).

To try on the host (not possible from the sandbox, no root): `rocm-smi --setperflevel high` on both cards before a
decode bench. On 05 Oct the waiting card's shader clock was seen dropping to ~1.5 GHz in the gaps between kernels and
staying at ~2.95 GHz only while a queued graph spun; a forced level would remove that effect, if the driver allows it
for RDNA3. Measure the eight-prompt decode speed with and without, and the power draw.
