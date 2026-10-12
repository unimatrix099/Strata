#!/usr/bin/env bash
# S1: the three split branches against current upstream/main (7695859a), exact text and decode, one and two GPUs.
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3
B=~/strata-tools/bench.py; M=~/strata-tools/bench_many.py
UP=/workspace/upstream-main/build-hip/strata
A=/workspace/pr-a/build-hip/strata; PB=/workspace/pr-b/build-hip/strata; PC=/workspace/pr-c/build-hip/strata
SWE="--env STRATA_HIP_POST_SPLIT=1 --env STRATA_HIP_FRONT_FUSE=1"
for G in 2 1; do
  if [ $G = 1 ]; then C=/workspace/strata-swift-iq3_s-1gpu.json; EX="--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
  else C=/workspace/strata-swift-iq3_s-2gpu-base.json; EX="--env STRATA_IQ_MT_MIN=1 --env STRATA_PF_FUSED=0 --env STRATA_HIP_WMMA=0 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"; fi
  [ $G = 2 ] && python3 $B --label S1-g$G-exact-a --reps 2 --config $C $EX --exe $A
  python3 $B --label S1-g$G-exact-b --reps 2 --config $C $EX --exe $PB
  python3 $B --label S1-g$G-exact-c --reps 2 --config $C $EX --exe $PC
  python3 $B --label S1-g$G-exact-csw --reps 2 --config $C $EX --exe $PC $SWE
  for r in 1 2; do
    python3 $M --label S1-g$G-up-$r --reps 1 --config $C --exe $UP
    [ $G = 2 ] && python3 $M --label S1-g$G-a-$r --reps 1 --config $C --exe $A
    python3 $M --label S1-g$G-b-$r --reps 1 --config $C --exe $PB
    python3 $M --label S1-g$G-csw-$r --reps 1 --config $C --exe $PC $SWE
  done
done
python3 - /workspace/strata-swift-iq3_s-2gpu-base.json "$PC" <<'PY'
import json, sys
c = json.load(open(sys.argv[1])); c["exe"] = sys.argv[2]
c["env"] = {**c.get("env", {}), "STRATA_HIP_POST_SPLIT": "1", "STRATA_HIP_FRONT_FUSE": "1"}
json.dump(c, open("/workspace/strata-s1-smoke.json", "w"), indent=1)
PY
echo "== smoke g2 (C + switches)"; SMOKE_CONFIG=/workspace/strata-s1-smoke.json python3 ~/strata-tools/smoke.py
echo S1DONE
