#!/usr/bin/env bash
# short loop step: 16K prompts (two chunks, ~15 s each after the start) and 256-token decode runs
cd /workspace
until grep -q FQDONE ~/strata-tools/fusedq.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s2-$label --out ~/strata-tools/longctx/s2-$label --tests speed --speed-sizes 16k --env STRATA_PREFILL_TIMING=1 "$@"; }
run fused --env STRATA_PF_FUSED=1
run fused-wmma --env STRATA_PF_FUSED=1 --env STRATA_HIP_WMMA=1
run fused-pafast --env STRATA_PF_FUSED=1 --env STRATA_PA_FAST=1
run fused-wmma-pafast --env STRATA_PF_FUSED=1 --env STRATA_HIP_WMMA=1 --env STRATA_PA_FAST=1
B=~/strata-tools/bench.py; O=~/strata-tools/data/results.jsonl
python3 $B --label S2-hcq8 --reps 1 --tokens 256 --env STRATA_HC_Q8=1 --out $O
python3 $B --label S2-base --reps 1 --tokens 256 --out $O
echo S2DONE
