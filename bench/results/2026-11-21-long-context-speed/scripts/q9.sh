#!/bin/bash
cd /workspace/ar1121
while ! /bin/grep -q "Q8 DONE" q8.txt; do sleep 20; done
export LD_LIBRARY_PATH=/opt/rocm/lib
for e in "X=1" "STRATA_TOPK_SPLIT=1" "STRATA_TOPK_DEC_REG=1"; do echo "IDLE $e"; env $e STRATA_TOPK_REF=1 STRATA_SCORES_THREAD=1 STRATA_QSA_ROWS=1 /workspace/Strata/build-hip-prod/qsa_scores_gfx1100_parity --dec 2>&1 | /bin/grep -E "^TIMING|^TOPK|^DEC|MISMATCH" | awk '{print $1,$2,$3,$4,$5,$6}'; done
unset LD_LIBRARY_PATH
P=/workspace/Strata/engine/strata-ar1121-b
T="STRATA_DECODE_TIMING=1 STRATA_VERIFY_PROFILE=1"
for kv in "G1-rows STRATA_QSA_ROWS=1" "G2-rows+split STRATA_QSA_ROWS=1@STRATA_TOPK_SPLIT=1" "G3-rows+decreg STRATA_QSA_ROWS=1@STRATA_TOPK_DEC_REG=1"; do
  set -- $kv
  ./exp.sh $1 $P "$T ${2//@/ }" "" | /bin/grep -vE "GTT|sha"
  python3 prof.py $1 | awk 'NR%2==1{a=$0} NR%2==0{print a" | "$3,$5}'
done
echo Q9 DONE
