# Strata (our fork, prod-1120 + autoresearch settings) vs Strata-HotRod  -- PLAN, not yet run

Security audit of sangharshadhyeta/Strata-HotRod (d7f9dc4, 21 Nov 2026): see ../hotrod-audit (src = clone). No malware found.
Blocked 21 Nov: /dev/kfd, /dev/dri, /opt/rocm missing in the container -> no build, no run.

Protocol (copied from compare-strata-0217 / compare-strata-vs-avalon):
- model: Swift 1.5 IQ3_S (same files in /workspace/Strata/Strata-data/models/swift-IQ3_S), 2x7900 XTX, --gpus 1,0
- fresh server per run, 2 rounds per engine
- `strata.py bench run` standard-v1 (4K/16K prompts, short, sanity 12) - same runner for both
- scripts/greedy.py: 8 prompts x3 x512, temp 0, thinking ON
- scripts/qeval0217.py: 79 known-answer items (quality; HotRod's "stand-ins" are NOT exact)
Engines: A = ours (engine/strata-prod-1120 + strata-swift-iq3_s-tuned.json); B = HotRod HEAD built with
  cmake -DSTRATA_ENABLE_HIP=ON -DCMAKE_HIP_ARCHITECTURES=gfx1100 (cmake from Strata .venv; HIP_PLATFORM=amd), two configs:
  B1 = HotRod defaults (self-tuning/calibration, learned look-ahead, stand-ins); B2 = same switches as ours, HotRod's extras off.
Caveat: HotRod README says tested on RTX 3060 only; its AMD paths unvalidated (README: turn LA_EXACT_W etc. off on AMD).

---
# RESULTS (21 Nov 2026) - 2x RX 7900 XTX, Swift 1.5 IQ3_S, same model files, same bench runner

Engines: **ours** = engine/strata-prod-1120 + strata-swift-iq3_s-tuned.json (as installed). **HotRod** = d7f9dc4 + a local HIP port
(hotrod-hip-port.patch, 3 files + 2 compat headers: HotRod does not build for AMD as published - native_router.cu CUDA-only
constructs, missing cuda->hip shim symbols, `__shared__ alignas`). hotrod-matched = our config minus the two switches HotRod lacks
(HIP_POST_SPLIT, FRONT_FUSE); hotrod-stock = matched minus our 3 autoresearch settings (prefill auto, reserve-later 3072, no SPLIT_RING/PREFILL_EQUAL/MTP_BATCH) plus STRATA_LA_LEARN=1.

| | ours A / B | hotrod-matched A / B | hotrod-stock A / B |
|---|---|---|---|
| greedy decode median of 8, tok/s | 117.9 / 116.8 | 100.2 / 97.2 | 103.7 / 105.8 |
| prefill 4.5K tok/s | 1387 / 1380 | 820 / 824 | 798 / 801 |
| prefill 36K tok/s | 3278 / 3259 | 1790 / 1746 | 1601 / 1576 |

standard-v1 (temperature 1.0, thinking on, 1 run, median): 
| | ours | hotrod-matched | hotrod-stock |
|---|---|---|---|
| decode short / 4K / 16K tok/s | 111 / 112 / 105 | 58 / 57 / 54 | 55 / 54 / 55 |
| prompt 4K / 16K tok/s | 1303 / 2600 | 804 / 1253 | 796 / 1221 |
| TTFT 4K / 16K s | 3.2 / 6.3 | 5.1 / 13.1 | 5.2 / 13.5 |
| sanity | 12/12 | 12/12 | 12/12 |

Known answers (79 items, greedy): ours 61/63 math + 16/16 code (90K tokens, 641 s); hotrod-matched 62/63 + 15/16 (83K tokens, 793 s); hotrod-stock 62/63 + 16/16 (72K tokens, 694 s; run after the 22 Nov machine restart).
=> no quality difference visible at this size (1 item each way).

Open points: (1) hotrod decode halves under temperature 1.0 (100 greedy -> 57 std suite) while ours stays ~111; cause not investigated
(draft acceptance under sampling is the first suspect). (2) [resolved by a machine restart; stock std+qeval rerun fine afterwards] HotRod's stock-config server hung in startup ("still starting" 600 s) in the
std-suite step after a previous engine had been killed 15 s earlier; the engine became an unkillable zombie with 4 threads spinning
(400% CPU each) and 8 GB of GPU1 held (pids 73902, 74450); a second engine started behind it hung the same way. Needs a container/GPU reset. The same config
had started fine 4 times before. (3) none of HotRod's own features (low-RAM tiers, stand-ins, learned look-ahead) matter on this
125 GB RAM box; the test shows its engine on hardware it was not built for. Not a verdict on its RTX 3060 / SSD-streaming use.

---
# Follow-up: why HotRod decodes ~45% slower under sampling (21 Nov, scripts/samp.py, 3 prompts x 400 tokens, tok/s per prompt)

| | greedy | temp 1.0, top_p .95, top_k 20 | temp 1.0 only |
|---|---|---|---|
| HotRod-matched (--pipeline-windows 2) | 119 / 97 / 113 | 69 / 57 / 59 | 66 / 54 / 58 |
| HotRod, --pipeline-windows removed | 64 / 57 / 63 | 61 / 55 / 60 | 62 / 54 / 56 |
| ours (prod-1120 config) | 148 / 107 / 123 | 129 / 93 / 117 | 121 / 100 / 118 |

- Draft acceptance is the same in every mode (~82% HotRod, ~85% ours): it is not worse drafting.
- Without the window pipeline HotRod's greedy speed falls to its sampled speed (~60). So in HotRod the 2-window pipeline only runs for
  greedy requests; any temperature > 0 uses the unpipelined path, which costs ~45% on this 2-GPU layer split. Ours pipelines sampled
  requests too (sampled 5-10% below greedy). HotRod's own docs report no such gap on its single RTX 3060 (no split, no pipeline there).
- Why it matters: the standard-v1 suite and normal chat use temperature 1.0, so HotRod's greedy numbers (100-106) overstate what a
  chat client sees on this box (~55-58). Where in HotRod's code the gate sits was not located (no pipeline/temperature match in src/program, serve).
