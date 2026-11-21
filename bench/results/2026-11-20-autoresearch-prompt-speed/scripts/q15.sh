#!/bin/bash
cd /workspace/ar1120
export PF_LONG=1
P=/workspace/Strata/engine/strata-prod-1120
B="STRATA_SPLIT_MTP_BATCH=1 STRATA_PREFILL_EQUAL=1"; R="--vram-reserve-later-mib 2048 --prefill 12288"
./exp.sh F6-shmax5120 $P "$B STRATA_PREFILL_CPU_SHARE_MAX=5120" "$R" 2 prefill
./exp.sh F7-shmax5120-nt64 $P "$B STRATA_PREFILL_CPU_SHARE_MAX=5120 STRATA_PREFILL_CPU_SHARE_NT=64" "$R" 2 prefill
echo Q15 DONE
