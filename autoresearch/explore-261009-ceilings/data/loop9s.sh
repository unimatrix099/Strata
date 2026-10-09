#!/usr/bin/env bash
# step 9: more cache on card 2 with the split pinned at 27 (step 7's auto split moved to 26 and ate the gain)
cd /workspace
until grep -q S8DONE ~/strata-tools/loop8s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
run() { label=$1; shift; python3 $L --label s9-$label --out ~/strata-tools/longctx/s9-$label --tests speed --speed-sizes 4k,32k --env STRATA_PREFILL_TIMING=1 "$@"; }
run split27 --arg=--layer-split --arg=27
run split27-later2048 --arg=--layer-split --arg=27 --drop-arg=--vram-reserve-later-mib --arg=--vram-reserve-later-mib --arg=2048
run split27-later1536 --arg=--layer-split --arg=27 --drop-arg=--vram-reserve-later-mib --arg=--vram-reserve-later-mib --arg=1536
echo S9DONE
