#!/usr/bin/env bash
# D7: the shared expert on its own graph branch (STRATA_SH_STREAM=1; off by default on HIP after gfx1201/gfx1030
# numbers, never measured on gfx1100 + ROCm 7.9) against the default, eight prompts, three alternating rounds, the
# production engine. The output is identical by construction (same kernels, same order of sums).
cd /workspace
sleep 20
pkill -f "serve/serve[r].py"
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 15
B=~/strata-tools/bench_many.py
for r in 1 2 3; do
python3 $B --label D7-base-$r --reps 1
python3 $B --label D7-shfork-$r --reps 1 --env STRATA_SH_STREAM=1
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D7DONE
