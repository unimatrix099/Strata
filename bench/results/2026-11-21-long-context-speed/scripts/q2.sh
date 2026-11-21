#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-prod-1121
T="STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1"
./exp.sh B1 $P "$T" ""
./exp.sh S1-scores-thread $P "$T STRATA_SCORES_THREAD=1" ""
./exp.sh K1-kv-prefetch $P "$T STRATA_KV_PREFETCH=1" ""
./exp.sh R1-qsa-rows $P "$T STRATA_QSA_ROWS=1" ""
./exp.sh KR0-kv-all-vram $P "$T" "--kv-resident 0"
echo Q2 DONE
