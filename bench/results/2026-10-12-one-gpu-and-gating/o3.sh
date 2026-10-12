#!/usr/bin/env bash
# O3 (the base rebuilt with STRATA_PREFILL_MMQ=ON, as setup builds): one GPU (HIP device 1, Swift 1.5 IQ3_S, setup's one-GPU config): the base engine (fb58e0d) against the fork's
# engine with its defaults and with STRATA_HIP_POST_SPLIT + STRATA_HIP_FRONT_FUSE
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3
B=~/strata-tools/bench.py; M=~/strata-tools/bench_many.py; L=~/strata-tools/longctx.py
C=/workspace/strata-swift-iq3_s-1gpu.json
BASE="--exe /workspace/upstream-base/build-hip/strata"
FORK="--exe /workspace/build-hip/strata"
SW="--exe /workspace/build-hip/strata --env STRATA_HIP_POST_SPLIT=1 --env STRATA_HIP_FRONT_FUSE=1"
EX="--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
python3 $B --label O3-exact-base --reps 2 --config $C $EX $BASE
python3 $B --label O3-exact-fork --reps 2 --config $C $EX $FORK
python3 $B --label O3-exact-sw --reps 2 --config $C $EX $SW
for r in 1 2 3; do
python3 $M --label O3-base-$r --reps 1 --config $C $BASE
python3 $M --label O3-fork-$r --reps 1 --config $C $FORK
python3 $M --label O3-sw-$r --reps 1 --config $C $SW
done
python3 $L --label o3-pf-base --out ~/strata-tools/longctx/o3-pf-base --config $C --exe /workspace/upstream-base/build-hip/strata --tests speed --speed-sizes 1k,4k,16k
python3 $L --label o3-pf-fork --out ~/strata-tools/longctx/o3-pf-fork --config $C --exe /workspace/build-hip/strata --tests speed --speed-sizes 1k,4k,16k
python3 $L --label o3-pf-sw --out ~/strata-tools/longctx/o3-pf-sw --config $C --exe /workspace/build-hip/strata --env STRATA_HIP_POST_SPLIT=1 --env STRATA_HIP_FRONT_FUSE=1 --tests speed --speed-sizes 1k,4k,16k
SMOKE_CONFIG=/workspace/strata-swift-iq3_s-1gpu-sw.json python3 ~/strata-tools/smoke.py
echo O3DONE
