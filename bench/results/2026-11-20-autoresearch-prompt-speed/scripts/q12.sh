#!/bin/bash
cd /workspace/ar1120
export PF_LONG=1
P=/workspace/Strata/engine/strata-prod-1120
B="STRATA_SPLIT_MTP_BATCH=1"; R="--vram-reserve-later-mib 2048"
./exp.sh F2-pf16k $P "$B STRATA_TRACE=1" "$R --prefill 16384 --prefill-pipe 8000" 2 prefill
./exp.sh F0-A $P "$B" "$R" 2 prefill
./exp.sh F1-pf12k $P "$B" "$R --prefill 12288 --prefill-pipe 8000" 2 prefill
./exp.sh F3-pf12k-ring192 $P "$B STRATA_SPLIT_RING=192" "$R --prefill 12288 --prefill-pipe 8000" 2 prefill
echo Q12 DONE
