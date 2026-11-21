#!/bin/bash
cd /workspace/ar1121
while ! /bin/grep -q "Q3 DONE" q3.txt; do sleep 20; done
P=/workspace/Strata/engine/strata-prod-1121
./exp.sh D-prof $P "STRATA_DECODE_TIMING=1 STRATA_VERIFY_PROFILE=1" ""
/bin/grep -E "GDN layers:|QSA layers:" run/D-prof-engine.txt | cut -c1-1500
echo Q4 DONE
