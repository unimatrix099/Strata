#!/usr/bin/env bash
# the rebased build (build-hip/strata) against the installed engine: exactness across pipeline / serial / early launch,
# then the installed config's speed over eight prompts
cd /workspace
B=bench/results/2026-10-05-split-decode-7900xtx/bench.py
M=autoresearch/explore-261007-tg/bench_many.py
O=autoresearch/explore-261007-tg/results.jsonl
X=(--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0 --out "$O" --exe /workspace/build-hip/strata --reps 1)
python3 $B --label RB-pw2 "${X[@]}"
python3 $B --label RB-serial "${X[@]}" --drop-arg=--pipeline-windows
python3 $B --label RB-noearly "${X[@]}" --env STRATA_SPLIT_EARLY_LAUNCH=0
for r in 1 2; do
  python3 $M --label RB-new-$r --reps 1 --exe /workspace/build-hip/strata --out "$O"
  python3 $M --label RB-old-$r --reps 1 --out "$O"
done
