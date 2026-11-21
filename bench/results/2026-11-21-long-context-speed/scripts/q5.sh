#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-prod-1121
T="STRATA_DECODE_TIMING=1 STRATA_VERIFY_PROFILE=1"
Z="8000,240000"
for kv in "rows STRATA_QSA_ROWS=1" "halve STRATA_SCORES_HALVE=1" "topkold STRATA_TOPK_OLD=1" "multi0 STRATA_SCORES_MULTI=0"; do
  set -- $kv
  ./exp.sh P-$1 $P "$T $2" "" 2 $Z | /bin/grep -v GTT
  python3 prof.py P-$1
done
echo Q5 DONE
