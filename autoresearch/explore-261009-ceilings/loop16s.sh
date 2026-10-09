#!/usr/bin/env bash
# step 16: smaller prompt chunks (2048 / 4096 against the default 8192): a 4K prompt then runs as two or more chunks
# pipelined across the cards; 32K checks the cost on long prompts (each chunk streams the non-resident experts)
cd /workspace
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s16-$label --out ~/strata-tools/longctx/s16-$label --tests speed --speed-sizes 2k,4k,32k --env STRATA_PREFILL_TIMING=1 "$@"; }
run chunk8192
run chunk4096 --drop-arg=--prefill --arg=--prefill --arg=4096
run chunk2048 --drop-arg=--prefill --arg=--prefill --arg=2048
echo S16DONE
