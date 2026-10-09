#!/usr/bin/env bash
# step 11: STRATA_PREFILL_CPU_SHARE=1 - a repeat of the 1K / 2K speed, then short-prompt quality (facts and needles at
# ~1K, the server smoke test)
cd /workspace
L=~/strata-tools/longctx.py
python3 $L --label s11-cpushare2 --out ~/strata-tools/longctx/s11-cpushare2 --tests speed --speed-sizes 1k,2k --env STRATA_PREFILL_CPU_SHARE=1
python3 $L --label s11-base2 --out ~/strata-tools/longctx/s11-base2 --tests speed --speed-sizes 1k,2k
python3 $L --label s11-cpushare-q --out ~/strata-tools/longctx/s11-cpushare-q --tests needles,facts --needle-sizes 1k --fact-sizes 1k --env STRATA_PREFILL_CPU_SHARE=1
STRATA_PREFILL_CPU_SHARE=1 SMOKE_EXE=/workspace/engine/strata python3 ~/strata-tools/vpy.py ~/strata-tools/smoke.py
echo S11DONE
