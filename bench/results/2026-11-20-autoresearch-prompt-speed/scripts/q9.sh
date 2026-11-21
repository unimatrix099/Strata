#!/bin/bash
cd /workspace/ar1120
while kill -0 71986 2>/dev/null; do sleep 2; done
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh pf10k $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_TRACE=1" "--prefill 10240 --prefill-pipe 8000" 2 prefill
./exp.sh pf12k $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_TRACE=1" "--prefill 12288 --prefill-pipe 8000" 2 prefill
echo Q9 DONE
