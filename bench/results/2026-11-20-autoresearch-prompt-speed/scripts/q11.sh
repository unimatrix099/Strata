#!/bin/bash
cd /workspace/ar1120
P=/workspace/Strata/engine/strata-prod-1120
B="STRATA_SPLIT_MTP_BATCH=1"; R="--vram-reserve-later-mib 2048"
./exp.sh pf10k $P "$B STRATA_TRACE=1" "$R --prefill 10240 --prefill-pipe 8000" 2 prefill
./exp.sh shnt64 $P "$B STRATA_PREFILL_TIMING=1 STRATA_PREFILL_CPU_SHARE_MAX=5120 STRATA_PREFILL_CPU_SHARE_NT=64" "$R" 2 prefill
./exp.sh ring192 $P "$B STRATA_SPLIT_RING=192" "$R" 2 prefill
./exp.sh shnt64all $P "$B STRATA_PREFILL_CPU_SHARE_MAX=9000 STRATA_PREFILL_CPU_SHARE_NT=64" "$R" 2 prefill
./exp.sh pf12k $P "$B STRATA_TRACE=1" "$R --prefill 12288 --prefill-pipe 8000" 2 prefill
./exp.sh ring48 $P "$B STRATA_SPLIT_RING=48" "$R" 2 prefill
echo Q11 DONE
