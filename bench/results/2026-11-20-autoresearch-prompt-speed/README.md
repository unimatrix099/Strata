# Autoresearch loop: prompt and decode speed on 2x RX 7900 XTX, engine 0.1.63 (20 Nov 2026)

A two-hour loop in the style of Karpathy's autoresearch: one fixed benchmark, one idea per run, keep what measures
better, discard the rest, log every run (`results.tsv`). Engine `engine/strata-prod-1120` (the 0.2.17 merge), Swift
1.5 IQ3_S, the production config `strata-swift-iq3_s-tuned.json`; every run is a fresh server (`scripts/exp.sh`).
Benchmark (`scripts/bench.py`): decode = 8 greedy prompts x 2 x 384 tokens, median of the per-prompt medians;
prompts = fresh 4.5K / 17.9K / 35.8K-token prompts, engine timings, median.

## Result

No engine code changed. Five settings went into the production config (backup
`strata-swift-iq3_s-tuned.json.before-ar1120`):

| setting | what it does |
|---|---|
| `STRATA_SPLIT_MTP_BATCH=1` | the draft layer's prompt K/V is built by the batched prompt path on the last card, not by the drafter's own 8-row pass (300 ms -> 50-90 ms per 8192-token chunk on the card that bounds the chunk cadence). The target's tokens are the same; drafts may differ. |
| `--vram-reserve-later-mib 2048` (was 3072) | the display card keeps 2 GiB instead of 3 for the desktop; its expert cache grows 8,312 -> 8,803 slots, so it streams fewer experts over PCIe per chunk. |
| `--prefill 12288` (was `auto` = 8192) | a larger chunk ceiling |
| `STRATA_SPLIT_RING=144` (was 96) | a larger ring of streamed-expert slots: the link runs further ahead of the kernels on long prompts (192: long prompts the same, 4.5K -2%). |
| `STRATA_PREFILL_EQUAL=1` | a prompt longer than one chunk is read in equal chunks (17.9K: 9216 + 8721 instead of 8192 + 8192 + 1553), and the slots lent to the prompt path are sized for that chunk, not for the ceiling. |

| prompt tok/s (median) | 4.5K | 17.9K | 35.8K |
|---|---|---|---|
| before (3 runs) | 1278-1281 | 2115-2159 | 2532-2547 |
| + batched draft K/V | 1313-1330 | 2273-2283 | 2652-2653 |
| + reserve 2048 | 1382-1398 | 2403-2406 | 2864-2876 |
| + 12288-token equal chunks (2 runs) | 1384-1400 | 2578 | 3102-3107 |
| + ring 144 (final; one test run, then the production server) | 1394 / 1376 | 2613 / 2648 | 3230 / 3240 |
| gain | **+8-9%** | **+22-24%** | **+27%** |

Decode is unchanged: before 116.8 / 110.2 / 113.8 / 119.7 tok/s over four starts, final 112.0 / 115.4 / 112.9 over
three (alternating starts; the start-to-start spread is about +-4%).

Quality check of the final config (`scripts/validate.py`): three needles at 10 / 50 / 90% depth in prompts of 9K,
22K, 45K and 112K tokens: 12 of 12 found (the 112K prompt read at 3,554 tok/s); six short known answers 6 of 6; no
engine error lines; decode after the long prompts 120-133 tok/s. The same check against the production server after
the install (port 8080, ring 144 included): 18 of 18, decode 116.1 tok/s, 112K prompt at 3,634 tok/s.

## Why these work: the prompt path on this PC is bound by the PCIe links

`STRATA_PREFILL_TIMING=1` and `STRATA_TRACE=1` on a 4.5K prompt (3.6 s): first card 1.2 s, second card 1.8 s, then
0.33 s refilling the 2,240 cache slots the prompt path borrowed, 0.07 s checkpoint, 0.13 s parking the previous
conversation. The second card held 8,312 of its 11,264 experts and lent 1,063 of those slots to the prompt path, so
it streamed about 4,000 experts (9 GB) per chunk over a PCIe 3.0 x8 link that carries 7.1 GB/s: 1.3 s of its 1.7-2.1 s
per chunk is the link. Everything that helped cuts bytes over that link or passes over the experts: more resident
experts on that card, fewer chunks, loans sized for the chunk actually read.

## Discarded (measured, no gain or worse)

| run | decode | 4.5K / 17.9K / 35.8K | |
|---|---|---|---|
| `STRATA_MTP_STAGE0=1` (draft layer on the first card, the engine's own start-up hint) | 96.4 | | -17% |
| RDNA4 decode set (ATTN_LANECELL, ATTN_MERGE_V2, TSUM, EXPDEC, SMALL_UNR=3, GR_Q32_G=2, GR_Q8_PFS) | 112.1 | | noise |
| `--spec 4`; `--spec-min-p 0.6` | 109.4; 109.2 | | noise |
| RDNA4 prompt set (GDN_HEAD, GDN_PP=2, GDN_CONVL2, GDN_NOY, CVEC_FUSE, TOPK_REG_MIN_BLOCKS) | | 1266 / 2129 / 2540 | none |
| `STRATA_PF_F16=1 STRATA_PF_DM16=1` | | 1218 / 2076 / 2440 | -4% |
| `--prefill-pipe 1`; `3000` (8192 chunks) | | 1381 / 2172 / 2595; 1288 / 2122 / 2556 | noise |
| `STRATA_PREFILL_HELP=1` (never engages on the fused ring) | | 1383 / 2125 / 2526 | none |
| `STRATA_PREFILL_ROUTED_STREAM=1` | | 1340 / 2134 / 2581 | noise |
| reserve 1536 | 112.2 | 1404 / 2364 / 2746 | worse than 2048 |
| `--prefill 10240 --prefill-pipe 8000` | | 1398 / 2449 / 2978 | less than equal chunks |
| `--prefill 12288 --prefill-pipe 8000` | | 1396 / 2415 / 3085 | 17.9K less than equal chunks |
| `--prefill 16384 --prefill-pipe 8000` (the loan is sized for 16384 whatever the planned chunk) | | 1400 / 2214 / 2890 | worse |
| `--prefill 14336` + equal | | 1395 / 2577 / 3082 | same as 12288 |
| `STRATA_SPLIT_RING=192` (on 8192 chunks; on the final config 1369 / 2638 / 3218) | | 1368 / 2468 / 2948 | +2.5% long, -2% 4K: 144 kept instead |
| final + CPU share `MAX=5120`; `MAX=5120 NT=64` | | 1308 / 2422 / 3025; 1420 / 2477 / 3018 | long worse |
| CPU share `MAX=5120 NT=64`; `MAX=9000 NT=64` | | 1419 / 2319 / 2858; 1432 / 2073 / 2586 | 4K +2-3%, long worse |

| final + `STRATA_HANDOFF_PIPE=4` | | 1403 / 2616 / 3244 | noise |
| final + `STRATA_PREFILL_COPY_WAIT_DEDUPE=1 STRATA_PREFILL_COPY_EVENT_EVERY=4` (2 runs) | | 1437, 1418 / 2676, 2678 / 3199, 3210 | 4.5K and 18K about +2.5%, 36K -0.7%: at the edge of the noise, not adopted; worth a longer test |

The installed config over five runs (three test starts, two of the production server): 4.5K 1376-1394, 17.9K
2557-2648, 35.8K 3222-3240 tok/s; decode on the production server 116.1 and 116.0.

Not possible here: a Q4_0 draft head (needs the BF16 MTP tensors; Hugging Face is not reachable from this sandbox).

## The display card's reserve

With the engine stopped the display card holds about 600 MiB for the desktop. At 3072 the card had 2.5 GiB free while
serving, at 2048 it has 1.5 GiB free and GTT use is unchanged (410 MiB). If the desktop ever needs more than about
2 GiB of VRAM (a game, a GPU-heavy app), the engine's memory would spill to GTT and decode would drop to ~15 tok/s
(05 Oct): set `--vram-reserve-later-mib 3072` again for such use. Check `/sys/class/drm/card1/device/mem_info_gtt_used`.

## Next ideas (code, not built)

1. Asynchronous refill: the lent slots hold the coldest experts of each cache; decode could start while they are
   copied back (0.33 s of a 4.5K prompt's 3.4 s, 0.64 s at 12K chunks). Needs the pipelined loop's fenced residency
   update (as the adaptive swaps), and the CPU lane rounds differently, so not bit-identical for the first windows.
2. The checkpoint after the batched read (70 ms) could overlap the refill.
3. `--prefill-pipe` on a ceiling above the planned chunk: size the loan for the planned chunk (today the ceiling).
4. A smaller prompt-path loan per token (about 0.27 slots = 0.6 MB a token) would cut streamed bytes directly.
