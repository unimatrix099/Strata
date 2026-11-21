#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-prod-1121
T="STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1"
Z="8000,240000"
./exp.sh S2-thread+prefetch $P "$T STRATA_SCORES_THREAD=1 STRATA_KV_PREFETCH=1" "" 2 $Z
./exp.sh M1-simt1 $P "$T STRATA_SELECT_SIMT=1" "" 2 $Z
./exp.sh M2-simt2 $P "$T STRATA_SELECT_SIMT=2" "" 2 $Z
./exp.sh S3-thread+topk-reg $P "$T STRATA_SCORES_THREAD=1 STRATA_TOPK_STREAM=0" "" 2 $Z
echo Q3 DONE
