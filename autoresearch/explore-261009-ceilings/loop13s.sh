#!/usr/bin/env bash
# step 13: PA_FAST's shorter quality check (needles 32K/128K, facts 118K, a 2K-token answer after 118K), and decode at
# 118K with the attention cache resident to 64K instead of 32K
cd /workspace
L=~/strata-tools/longctx.py
python3 $L --label s13-pafast-q --out ~/strata-tools/longctx/s13-pafast-q --env STRATA_PA_FAST=1 --tests needles,facts,longans --needle-sizes 32k,128k --fact-sizes 118k --longans-sizes 118k --longans-tokens 2000
python3 $L --label s13-kv64k --out ~/strata-tools/longctx/s13-kv64k --tests longans --longans-sizes 118k --longans-tokens 1500 --drop-arg=--kv-resident --arg=--kv-resident --arg=65536
python3 $L --label s13-kv32k --out ~/strata-tools/longctx/s13-kv32k --tests longans --longans-sizes 118k --longans-tokens 1500
echo S13DONE
