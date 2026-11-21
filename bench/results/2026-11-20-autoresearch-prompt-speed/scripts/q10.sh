#!/bin/bash
cd /workspace/ar1120
while kill -0 72212 2>/dev/null; do sleep 2; done
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh ring192 $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_SPLIT_RING=192" "--vram-reserve-later-mib 2048" 2 prefill
./exp.sh ring48 $P "STRATA_SPLIT_MTP_BATCH=1 STRATA_SPLIT_RING=48" "--vram-reserve-later-mib 2048" 2 prefill
echo Q10 DONE
