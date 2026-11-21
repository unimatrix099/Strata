#!/bin/bash
cd /workspace/ar1120
while ps -eo args | grep -q "[q]1.sh"; do sleep 2; done
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh pftiming $P "STRATA_PREFILL_TIMING=1" "" 2 prefill
./exp.sh decprof $P "STRATA_VERIFY_PROFILE=1 STRATA_DECODE_TIMING=1" "" 1 decode
echo Q2 DONE
