#!/usr/bin/env bash
# F4: STRATA_PL_ASYNC_LAUNCH=1 (build-hip/strata) against the installed engine: exact protocol, then eight prompts x 3
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
B=~/strata-tools/bench.py
M=~/strata-tools/bench_many.py
NEW="--exe /workspace/build-hip/strata --env STRATA_PL_ASYNC_LAUNCH=1"
EX="--env STRATA_IQ_MT_MIN=1 --env STRATA_PF_FUSED=0 --env STRATA_HIP_WMMA=0 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
python3 $B --label F4-exact-old --reps 2 $EX
python3 $B --label F4-exact-new --reps 2 $EX $NEW
for r in 1 2 3; do
python3 $M --label F4-old-$r --reps 1
python3 $M --label F4-new-$r --reps 1 $NEW
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo F4DONE
