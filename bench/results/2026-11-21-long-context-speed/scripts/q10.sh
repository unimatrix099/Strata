#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-ar1121-c
N="STRATA_SCORES_THREAD=1 STRATA_KV_PREFETCH=1 STRATA_QSA_ROWS=1"
./exp.sh H1-long-prof $P "STRATA_DECODE_TIMING=1 STRATA_VERIFY_PROFILE=1 STRATA_QSA_ROWS=1 STRATA_SELECT_LONG=1" "" | /bin/grep -vE "GTT|sha"
python3 prof.py H1-long-prof | awk 'NR%2==1{a=$0} NR%2==0{print a" | "$3,$5}'
T="STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1"
export EXTRA_BENCH=1
./exp.sh F-c-base-1 $P "$T" "" | /bin/grep -vE "sha|TOTAL|DEEP|GTT"
VALIDATE=1 ./exp.sh F-c-long-1 $P "$T $N STRATA_SELECT_LONG=1" "" | /bin/grep -vE "sha|TOTAL|DEEP|GTT"
./exp.sh F-c-new-2 $P "$T $N" "" | /bin/grep -vE "sha|TOTAL|DEEP|GTT"
./exp.sh F-c-long-2 $P "$T $N STRATA_SELECT_LONG=1" "" | /bin/grep -vE "sha|TOTAL|DEEP|GTT"
echo Q10 DONE
