#!/usr/bin/env bash
# step 12: the CPU share's setting - auto (per-layer balance, chunks to 3K) against the fixed 0.5 and 1 - 1K / 2K / 4K
cd /workspace
until grep -q S11DONE ~/strata-tools/loop11s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s12-$label --out ~/strata-tools/longctx/s12-$label --tests speed --speed-sizes 1k,2k,4k --env STRATA_PREFILL_TIMING=1 "$@"; }
run auto --env STRATA_PREFILL_CPU_SHARE=auto
run share05 --env STRATA_PREFILL_CPU_SHARE=0.5
run share1 --env STRATA_PREFILL_CPU_SHARE=1
run base
echo S12DONE
