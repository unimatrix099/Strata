#!/bin/bash
cd /workspace/ar1120
P=/workspace/Strata/engine/strata-prod-1120
./exp.sh pftiming $P "STRATA_PREFILL_TIMING=1" "" 2 prefill
./exp.sh decprof $P "STRATA_VERIFY_PROFILE=1 STRATA_DECODE_TIMING=1" "" 1 decode
echo Q2 DONE
./exp.sh spec4 $P "" "--spec 4" 2 decode
./exp.sh minp06 $P "" "--spec-min-p 0.6" 2 decode
./exp.sh minp08 $P "" "--spec-min-p 0.8" 2 decode
./exp.sh theta005 $P "STRATA_PIPELINE_THETA=0.05" "" 2 decode
./exp.sh theta02 $P "STRATA_PIPELINE_THETA=0.2" "" 2 decode
./exp.sh pcie0 $P "" "--pcie-frac 0" 2 decode
./exp.sh pcieauto $P "STRATA_PCIE_FRAC_DEFAULT=auto" "" 2 decode
./exp.sh base-3 $P "" "" 2 decode
echo Q3 DONE
