#!/bin/bash
cd /workspace/ar1120
export PF_LONG=1
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh G1-handoff4 $P "STRATA_HANDOFF_PIPE=4" "" 2 prefill
./exp.sh G2-copyev $P "STRATA_PREFILL_COPY_WAIT_DEDUPE=1 STRATA_PREFILL_COPY_EVENT_EVERY=4" "" 2 prefill
echo Q16 DONE
