#!/usr/bin/env bash
# step 8: the idle card's help needs the MMQ prompt path (not the fused kernels): PF_FUSED=0 with and without it, 1K-4K
cd /workspace
until grep -q S7DONE ~/strata-tools/loop7s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s8-$label --out ~/strata-tools/longctx/s8-$label --tests speed --speed-sizes 1k,2k,4k --env STRATA_PREFILL_TIMING=1 --env STRATA_PF_FUSED=0 "$@"; }
run mmq-base
run mmq-help --env STRATA_PREFILL_HELP=1
echo S8DONE
