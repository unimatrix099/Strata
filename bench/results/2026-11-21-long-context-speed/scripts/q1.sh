#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-prod-1121
./exp.sh B0 $P "" ""
./exp.sh B0-timing $P "STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1" ""
echo Q1 DONE
