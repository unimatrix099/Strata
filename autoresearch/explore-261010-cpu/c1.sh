#!/usr/bin/env bash
# C1: the CPU pool's placement and task count (decode): 7 workers (CCD0 with the host) / 16 or 32 tasks per phase,
# against the production config's 15 workers / 48 tasks; eight prompts, three alternating rounds; plus a timing run
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
M=~/strata-tools/bench_many.py
B=~/strata-tools/bench.py
python3 $B --label C1-timing --reps 1 --env STRATA_DECODE_TIMING=1
for r in 1 2 3; do
python3 $M --label C1-base-$r --reps 1
python3 $M --label C1-pw7-$r --reps 1 --arg=--pool-workers --arg=7
python3 $M --label C1-pt16-$r --reps 1 --arg=--pool-tasks --arg=16
python3 $M --label C1-pt32-$r --reps 1 --arg=--pool-tasks --arg=32
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo C1DONE
