#!/usr/bin/env bash
# step 7: card 2 streams ~22% of its experts per prompt chunk (78% resident against card 1's 95%) - more cache on the
# desktop card: --vram-reserve-later-mib 2048 / 1536 against 3072, 32K prompt (fused + wmma base), one run each
cd /workspace
until grep -q S6DONE ~/strata-tools/loop6s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s7-$label --out ~/strata-tools/longctx/s7-$label --tests speed --speed-sizes 32k --env STRATA_PREFILL_TIMING=1 "$@"; }
run base
run later2048 --drop-arg=--vram-reserve-later-mib --arg=--vram-reserve-later-mib --arg=2048
run later1536 --drop-arg=--vram-reserve-later-mib --arg=--vram-reserve-later-mib --arg=1536
echo S7DONE
