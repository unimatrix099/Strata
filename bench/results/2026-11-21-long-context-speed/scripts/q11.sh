#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-ar1121-c
T="STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1"
N="STRATA_SCORES_THREAD=1 STRATA_KV_PREFETCH=1 STRATA_QSA_ROWS=1"
for i in 1 2; do
./exp.sh X-long-$i $P "$T $N STRATA_SELECT_LONG=1" "" 2 8000 | /bin/grep -E "CTX "
./exp.sh X-base-$i $P "$T" "" 2 8000 | /bin/grep -E "CTX "
done
echo Q11 DONE
