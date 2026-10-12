#!/usr/bin/env bash
cd /workspace
B=~/strata-tools/bench.py; M=~/strata-tools/bench_many.py
C1=/workspace/strata-swift-iq3_s-1gpu.json; C2=/workspace/strata-swift-iq3_s-2gpu-base.json
EX="--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
UP=/workspace/upstream-main/build-hip/strata; A=/workspace/pr-a/build-hip/strata; PB=/workspace/pr-b/build-hip/strata
python3 $B --label R2-up --reps 12 --config $C1 $EX --exe $UP
python3 $B --label R2-b --reps 12 --config $C1 $EX --exe $PB
for r in 3 4 5; do
  python3 $M --label S1-g2-up-$r --reps 1 --config $C2 --exe $UP
  python3 $M --label S1-g2-a-$r --reps 1 --config $C2 --exe $A
  python3 $M --label S1-g2-b-$r --reps 1 --config $C2 --exe $PB
done
echo R2DONE
