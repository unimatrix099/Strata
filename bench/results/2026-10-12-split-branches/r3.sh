#!/usr/bin/env bash
# R3: two GPUs WITHOUT --pipeline-windows (upstream's default for a two-stage gfx11 split): A's early launch and async commit
cd /workspace
B=~/strata-tools/bench.py; M=~/strata-tools/bench_many.py; C2=/workspace/strata-swift-iq3_s-2gpu-base.json
EX="--env STRATA_IQ_MT_MIN=1 --env STRATA_PF_FUSED=0 --env STRATA_HIP_WMMA=0 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
NP="--drop-arg=--pipeline-windows"
UP=/workspace/upstream-main/build-hip/strata; A=/workspace/pr-a/build-hip/strata
python3 $B --label R3-exact-up --reps 2 --config $C2 $EX $NP --exe $UP
python3 $B --label R3-exact-a --reps 2 --config $C2 $EX $NP --exe $A
for r in 1 2 3; do
  python3 $M --label R3-up-$r --reps 1 --config $C2 $NP --exe $UP
  python3 $M --label R3-a-$r --reps 1 --config $C2 $NP --exe $A
done
echo R3DONE
