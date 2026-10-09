#!/usr/bin/env bash
# step 14: the PLE table in RAM (--ple-io ram) against the SSD reads (direct, the default): 32K and 4K prompts
cd /workspace
until grep -q LONG1DONE ~/strata-tools/long1.txt; do sleep 30; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s14-$label --out ~/strata-tools/longctx/s14-$label --tests speed --speed-sizes 4k,32k --env STRATA_PREFILL_TIMING=1 "$@"; }
run ple-ram --arg=--ple-io --arg=ram
run ple-direct
run ple-ram2 --arg=--ple-io --arg=ram
echo S14DONE
