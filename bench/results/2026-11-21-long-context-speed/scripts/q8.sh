#!/bin/bash
cd /workspace/ar1121
P=/workspace/Strata/engine/strata-prod-1121
T="STRATA_PREFILL_TIMING=1 STRATA_DECODE_TIMING=1"
N="STRATA_SCORES_THREAD=1 STRATA_KV_PREFETCH=1 STRATA_QSA_ROWS=1"
./exp.sh C131-maxctx131k $P "STRATA_DECODE_TIMING=1 STRATA_VERIFY_PROFILE=1" "--max-context 131072" 2 8000,120000 | /bin/grep -vE "GTT|sha"
python3 prof.py C131-maxctx131k
export EXTRA_BENCH=1
for i in 1 2; do
  ./exp.sh F-base-$i $P "$T" "" | /bin/grep -vE "sha"
  ./exp.sh F-new-$i $P "$T $N" "" | /bin/grep -vE "sha"
done
echo Q8 DONE
