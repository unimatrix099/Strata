#!/bin/bash
cd /workspace/ar1120
P=/workspace/Strata/engine/strata-prod-1120
B="STRATA_SPLIT_MTP_BATCH=1 STRATA_PREFILL_EQUAL=1"; R="--vram-reserve-later-mib 2048 --prefill 12288"
VALIDATE=1 ./exp.sh final-1 $P "$B" "$R" 2 all
./exp.sh base-4 $P "" "" 2 decode
./exp.sh final-2 $P "$B" "$R" 2 decode
./exp.sh base-5 $P "" "" 2 decode
./exp.sh final-3 $P "$B" "$R" 2 decode
echo Q14 DONE
