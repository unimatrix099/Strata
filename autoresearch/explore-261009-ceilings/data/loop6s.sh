#!/usr/bin/env bash
# step 6: the idle card's help on one-chunk prompts (1K-4K), fused base, and the share fixed at 0.5
cd /workspace
until grep -q S5DONE ~/strata-tools/loop5s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s6-$label --out ~/strata-tools/longctx/s6-$label --tests speed --speed-sizes 1k,2k,4k --env STRATA_PREFILL_TIMING=1 "$@"; }
run help --env STRATA_PREFILL_HELP=1
run help-frac05 --env STRATA_PREFILL_HELP=1 --env STRATA_PREFILL_HELP_FRAC=0.5
run base
echo S6DONE
