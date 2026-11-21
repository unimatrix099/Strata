#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-prod-1121
T="STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1"
./exp.sh Bsha-a $P "$T" "" 2 8000,128000
./exp.sh Bsha-b $P "$T" "" 2 8000,128000
VALIDATE=1 ./exp.sh V1-thread+prefetch+rows $P "$T STRATA_SCORES_THREAD=1 STRATA_KV_PREFETCH=1 STRATA_QSA_ROWS=1" ""
echo Q6 DONE
