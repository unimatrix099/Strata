#!/usr/bin/env bash
# R4: two GPUs without --pipeline-windows, exact protocol, 8 reps: does A's text repeat, and which part changes it?
until grep -q R3DONE ~/strata-tools/r3.out; do sleep 20; done
cd /workspace
B=~/strata-tools/bench.py; C2=/workspace/strata-swift-iq3_s-2gpu-base.json
EX="--env STRATA_IQ_MT_MIN=1 --env STRATA_PF_FUSED=0 --env STRATA_HIP_WMMA=0 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0 --drop-arg=--pipeline-windows"
UP=/workspace/upstream-main/build-hip/strata; A=/workspace/pr-a/build-hip/strata
python3 $B --label R4-up --reps 8 --config $C2 $EX --exe $UP
python3 $B --label R4-a --reps 8 --config $C2 $EX --exe $A
python3 $B --label R4-a-noearly --reps 8 --config $C2 $EX --exe $A --env STRATA_SPLIT_EARLY_LAUNCH=0
python3 $B --label R4-a-sync --reps 8 --config $C2 $EX --exe $A --env STRATA_SPLIT_COMMIT_SYNC=1
echo R4DONE
