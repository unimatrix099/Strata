#!/usr/bin/env bash
# prompt-path variants on a 32K prompt (the answer's text kept for exactness) and the 4.6K prefill bench for the helper
cd /workspace
until grep -q VPDONE ~/strata-tools/vprof.txt; do sleep 10; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label pf-$label --out ~/strata-tools/longctx/pf-$label --tests speed --speed-sizes 32k --env STRATA_PREFILL_TIMING=1 "$@"; }
run base
run fused --env STRATA_PF_FUSED=1
run gemm --env STRATA_PF_GEMM=1
run chunk16k --drop-arg=--prefill --arg=--prefill --arg=16384
run base2
B=~/strata-tools/bench.py; O=~/strata-tools/data/results.jsonl
python3 $B --label PFH-base --prefill --reps 3 --out $O
python3 $B --label PFH-help --prefill --reps 3 --env STRATA_PREFILL_HELP=1 --out $O
python3 $B --label PFH-base2 --prefill --reps 3 --out $O
echo PFVDONE
