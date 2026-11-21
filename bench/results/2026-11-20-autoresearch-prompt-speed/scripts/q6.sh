#!/bin/bash
cd /workspace/ar1120
while pgrep -f "q5.sh" > /dev/null; do sleep 2; done
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh trace $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_TRACE=1 STRATA_PREFILL_TIMING=1" "" 2 prefill
echo Q6 DONE
