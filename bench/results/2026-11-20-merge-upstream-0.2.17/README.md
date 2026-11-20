# Upstream Strata 0.2.17 on 2x RX 7900 XTX (20 Nov 2026)

Same protocol as the Avalon comparison (standard-v1 via the old-tree runner, greedy.py 8 prompts x3 x512, fresh server per run,
2 rounds each, Swift IQ3_S production config, scripts/runeng.sh). Engines: prod = engine/strata-prod-1112 (0.2.8 merge + fork);
up = plain upstream/main 3142b66b9 (0.1.63, no fork patches, worktree /workspace/up0217); merged = branch merge-upstream-0.2.17
(0204971c8, worktree /workspace/merge0217, NOT pushed, NOT installed).

| | prod A / B | upstream alone A / B | merged A / B |
|---|---|---|---|
| prefill 4K tok/s (standard) | 1142 / 1148 | 1195 / 1208 | 1208 / 1196 |
| prefill 16K tok/s | 2207 / 2201 | 2259 / 2258 | 2264 / 2256 |
| TTFT 4K / 16K s | 3.60 / 7.46 | 3.42 / 7.30 | 3.42-3.45 / 7.27-7.30 |
| decode, standard 4K / 16K / short tok/s | 99-109 / 93-98 / 97-102 | 92-104 / 87-95 / 93-99 | 104-115 / 105-112 / 105 |
| decode, greedy median of 8 tok/s | 109.4 / 116.2 | 106.0 / 106.6 | 114.2 / 118.7 |
| decode, greedy mean of 8 | 114.8 / 117.9 | 109.1 / 108.7 | 119.4 / 119.6 |
| sanity 12 | 12/12 | 12/12 | 12/12 |

Known answers (qeval both, 79 items): merged 62/63 math + 16/16 code (the miss: sum of primes < 670, answered 21536, ref 36888);
0.2.8 merge 63/63, installed 12 Oct engine 61/63. Outlier: merged-B's 32-token read after the 32K prompt decoded at 60.6 tok/s (A: 97.9).
