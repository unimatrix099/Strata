#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-ar1121-dec
T="STRATA_DECODE_TIMING=1 STRATA_VERIFY_PROFILE=1"
for kv in "E0-newbin-base X=1" "E1-dec STRATA_SCORES_DEC=1" "E2-dec+rows-off-check STRATA_SCORES_DEC=1"; do
  set -- $kv
  [ $1 = E2-dec+rows-off-check ] && continue
  ./exp.sh $1 $P "$T $2" "" | /bin/grep -vE "GTT|sha"
  python3 prof.py $1
done
echo Q7 DONE
