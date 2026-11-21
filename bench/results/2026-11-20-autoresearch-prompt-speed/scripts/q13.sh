#!/bin/bash
cd /workspace/ar1120
export PF_LONG=1
P=/workspace/Strata/engine/strata-prod-1120
B="STRATA_SPLIT_MTP_BATCH=1"; R="--vram-reserve-later-mib 2048"
./exp.sh F4-eq12k $P "$B STRATA_PREFILL_EQUAL=1 STRATA_TRACE=1" "$R --prefill 12288" 2 prefill
./exp.sh F5-eq14k $P "$B STRATA_PREFILL_EQUAL=1" "$R --prefill 14336" 2 prefill
echo Q13 DONE
