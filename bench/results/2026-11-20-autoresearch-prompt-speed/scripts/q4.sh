#!/bin/bash
cd /workspace/ar1120
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh splitmtp $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_PREFILL_TIMING=1" "" 2 all
./exp.sh spec4 $P "" "--spec 4" 2 decode
./exp.sh minp06 $P "" "--spec-min-p 0.6" 2 decode
./exp.sh theta02 $P "STRATA_PIPELINE_THETA=0.2" "" 2 decode
./exp.sh pcie0 $P "" "--pcie-frac 0" 2 decode
./exp.sh base-3 $P "" "" 2 decode
echo Q4 DONE
