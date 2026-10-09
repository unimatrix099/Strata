#!/usr/bin/env bash
# short loop step 3, on the fused base: short prompts with STREAM_MIN=128, HC_UPMIX / PF_PAD at 16K, 16K chunks and
# a later layer split at 32K (card 2 bounds the chunk cadence under the fused path), each one engine start
cd /workspace
until grep -q S2DONE ~/strata-tools/loop2s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; sizes=$2; shift 2; python3 $L --label s3-$label --out ~/strata-tools/longctx/s3-$label --tests speed --speed-sizes $sizes --env STRATA_PREFILL_TIMING=1 "$@"; }
run short-base 1k,2k,4k
run short-stream128 1k,2k,4k --env STRATA_PREFILL_STREAM_MIN=128
run upmix 16k --env STRATA_HC_UPMIX=1
run pfpad 16k --env STRATA_PF_PAD=1
run base32 32k
run chunk16k 32k --drop-arg=--prefill --arg=--prefill --arg=16384
run split28 32k --arg=--layer-split --arg=28
run split30 32k --arg=--layer-split --arg=30
echo S3DONE
