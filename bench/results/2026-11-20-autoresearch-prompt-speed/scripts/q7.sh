#!/bin/bash
cd /workspace/ar1120
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh trace $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_TRACE=1 STRATA_PREFILL_TIMING=1" "" 2 prefill
./exp.sh res2048 $P "STRATA_SPLIT_MTP_BATCH=1" "--vram-reserve-later-mib 2048" 2 all
./exp.sh res1536 $P "STRATA_SPLIT_MTP_BATCH=1" "--vram-reserve-later-mib 1536" 2 all
echo Q7 DONE
