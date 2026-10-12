#!/usr/bin/env bash
# P1: current upstream/main (7695859a) against the proposal branch (upstream/main + the fork's code), defaults and with
# STRATA_HIP_POST_SPLIT + STRATA_HIP_FRONT_FUSE; one GPU (setup's one-GPU config) and two GPUs (the production tuning
# without the two switches). All engines built as setup builds (STRATA_PREFILL_MMQ=ON).
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3
B=~/strata-tools/bench.py; M=~/strata-tools/bench_many.py; L=~/strata-tools/longctx.py
UP=/workspace/upstream-main/build-hip/strata
PR=/workspace/pr-branch/build-hip/strata
SWE="--env STRATA_HIP_POST_SPLIT=1 --env STRATA_HIP_FRONT_FUSE=1"
for G in 1 2; do
  if [ $G = 1 ]; then C=/workspace/strata-swift-iq3_s-1gpu.json; EX="--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"; SZ=1k,4k,16k
  else C=/workspace/strata-swift-iq3_s-2gpu-base.json; EX="--env STRATA_IQ_MT_MIN=1 --env STRATA_PF_FUSED=0 --env STRATA_HIP_WMMA=0 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"; SZ=4k,32k; fi
  python3 $B --label P1-g$G-exact-up --reps 2 --config $C $EX --exe $UP
  python3 $B --label P1-g$G-exact-pr --reps 2 --config $C $EX --exe $PR
  python3 $B --label P1-g$G-exact-sw --reps 2 --config $C $EX --exe $PR $SWE
  for r in 1 2 3; do
    python3 $M --label P1-g$G-up-$r --reps 1 --config $C --exe $UP
    python3 $M --label P1-g$G-pr-$r --reps 1 --config $C --exe $PR
    python3 $M --label P1-g$G-sw-$r --reps 1 --config $C --exe $PR $SWE
  done
  python3 $L --label p1-g$G-pf-up --out ~/strata-tools/longctx/p1-g$G-pf-up --config $C --exe $UP --tests speed --speed-sizes $SZ
  python3 $L --label p1-g$G-pf-pr --out ~/strata-tools/longctx/p1-g$G-pf-pr --config $C --exe $PR --tests speed --speed-sizes $SZ
  python3 $L --label p1-g$G-pf-sw --out ~/strata-tools/longctx/p1-g$G-pf-sw --config $C --exe $PR $SWE --tests speed --speed-sizes $SZ
  python3 - "$C" "$PR" <<'PY'
import json, sys
c = json.load(open(sys.argv[1])); c["exe"] = sys.argv[2]
c["env"] = {**c.get("env", {}), "STRATA_HIP_POST_SPLIT": "1", "STRATA_HIP_FRONT_FUSE": "1"}
json.dump(c, open("/workspace/strata-p1-smoke.json", "w"), indent=1)
PY
  echo "== smoke g$G"; SMOKE_CONFIG=/workspace/strata-p1-smoke.json python3 ~/strata-tools/smoke.py
done
echo P1DONE
