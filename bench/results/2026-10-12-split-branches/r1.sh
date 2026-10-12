#!/usr/bin/env bash
# R1: one GPU, exact protocol, 6 reps (one server each): does the story text repeat? upstream, B, B with the merged combine off
until grep -q S1DONE ~/strata-tools/s1.out; do sleep 20; done
cd /workspace
B=~/strata-tools/bench.py; C=/workspace/strata-swift-iq3_s-1gpu.json
EX="--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
python3 $B --label R1-up --reps 6 --config $C $EX --exe /workspace/upstream-main/build-hip/strata
python3 $B --label R1-b --reps 6 --config $C $EX --exe /workspace/pr-b/build-hip/strata
python3 $B --label R1-b-nogather --reps 6 --config $C $EX --exe /workspace/pr-b/build-hip/strata --env STRATA_COMBINE_GATHER=0
echo R1DONE
