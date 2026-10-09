#!/usr/bin/env bash
# step 10: short prompts on the production path - the CPU share of a small chunk's experts (upstream: one-GPU CUDA by
# default) and the fused experts from 128 tokens, 1K / 2K prompts
cd /workspace
until grep -q S9DONE ~/strata-tools/loop9s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s10-$label --out ~/strata-tools/longctx/s10-$label --tests speed --speed-sizes 1k,2k --env STRATA_PREFILL_TIMING=1 "$@"; }
run base
run cpushare --env STRATA_PREFILL_CPU_SHARE=1
run stream128 --env STRATA_PREFILL_STREAM_MIN=128
echo S10DONE
