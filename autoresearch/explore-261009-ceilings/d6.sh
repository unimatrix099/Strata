#!/usr/bin/env bash
# D6: the first draft always (STRATA_SPEC_MIN_P0=0) against the unchanged rule, eight prompts, three more alternating
# pairs (D5's rounds 2-3 gave 0.964 / 1.067 - inside the noise). build-hip/strata for both.
cd /workspace
pkill -f "serve/serve[r].py"
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 15
B=~/strata-tools/bench_many.py
X="--exe /workspace/build-hip/strata --reps 1"
for r in 4 5 6; do
python3 $B --label D5-base-$r $X
python3 $B --label D5-p0-0-$r $X --env STRATA_SPEC_MIN_P0=0
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D6DONE
