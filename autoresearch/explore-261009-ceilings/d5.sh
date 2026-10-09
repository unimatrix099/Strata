#!/usr/bin/env bash
# D5: the eight-prompt bench (bench_many, 512 tokens, 1 rep each) in three rounds, alternating the unchanged rule with the
# candidates: the first draft always (STRATA_SPEC_MIN_P0=0), min-p 0.4 for every draft, the first draft at 0.3.
# build-hip/strata for all. Runs after D4.
until grep -q D4DONE ~/strata-tools/d4.out; do sleep 15; done
cd /workspace
pkill -f "serve/serve[r].py" ; sleep 8
B=~/strata-tools/bench_many.py
X="--exe /workspace/build-hip/strata --reps 1"
for r in 1 2 3; do
python3 $B --label D5-base-$r $X
python3 $B --label D5-p0-0-$r $X --env STRATA_SPEC_MIN_P0=0
python3 $B --label D5-mp04-$r $X --drop-arg=--spec-min-p --arg=--spec-min-p --arg=0.4
python3 $B --label D5-p0-03-$r $X --env STRATA_SPEC_MIN_P0=0.3
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D5DONE
