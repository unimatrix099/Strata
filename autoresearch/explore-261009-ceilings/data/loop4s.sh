#!/usr/bin/env bash
# step 4: the block scorer on matrix cores on top of fused+wmma (16K), then a shorter quality check of fused+wmma
cd /workspace
until grep -q S3DONE ~/strata-tools/loop3s.txt; do sleep 15; done
L=~/strata-tools/longctx.py
python3 $L --label s4-wmma-select --out ~/strata-tools/longctx/s4-wmma-select --tests speed --speed-sizes 16k --env STRATA_PREFILL_TIMING=1 --env STRATA_HIP_WMMA=1 --env STRATA_SELECT_WMMA=1
python3 $L --label s4-wmma-quality --out ~/strata-tools/longctx/s4-wmma-quality --env STRATA_HIP_WMMA=1 --tests needles,facts,longans,turns --needle-sizes 32k,128k --fact-sizes 64k,118k --longans-sizes 118k --longans-tokens 2000
echo S4DONE
