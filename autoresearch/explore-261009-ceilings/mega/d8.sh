#!/usr/bin/env bash
# D8: the mixer's side branches (STRATA_DF_BRANCH=1: a/b and z beside q/k/v on a side stream; off by default after an
# NVIDIA Linux hang, never measured on HIP) and both branches together, against the default; eight prompts, three
# alternating rounds, the production engine. Runs after D7.
until grep -q D7DONE ~/strata-tools/d7.out; do sleep 15; done
cd /workspace
sleep 5
pkill -f "serve/serve[r].py"
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 15
B=~/strata-tools/bench_many.py
for r in 1 2 3; do
python3 $B --label D8-base-$r --reps 1
python3 $B --label D8-dfbranch-$r --reps 1 --env STRATA_DF_BRANCH=1
python3 $B --label D8-both-$r --reps 1 --env STRATA_DF_BRANCH=1 --env STRATA_SH_STREAM=1
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D8DONE
