#!/usr/bin/env bash
# C5: the CPU share limited to short chunks (STRATA_PREFILL_CPU_SHARE_MAX 1024 / 2048) against off and the production
# setting (share 1, limit 3072); 1K / 2K / 4K / 16K, two rounds
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
L=~/strata-tools/longctx.py
for r in 1 2; do
python3 $L --label c5-m1024-$r --out ~/strata-tools/longctx/c5-m1024-$r --tests speed --speed-sizes 1k,2k,4k,16k --env STRATA_PREFILL_CPU_SHARE=1 --env STRATA_PREFILL_CPU_SHARE_MAX=1024
python3 $L --label c5-m2048-$r --out ~/strata-tools/longctx/c5-m2048-$r --tests speed --speed-sizes 1k,2k,4k,16k --env STRATA_PREFILL_CPU_SHARE=1 --env STRATA_PREFILL_CPU_SHARE_MAX=2048
python3 $L --label c5-off-$r --out ~/strata-tools/longctx/c5-off-$r --tests speed --speed-sizes 1k,2k,4k,16k --env STRATA_PREFILL_CPU_SHARE=0
python3 $L --label c5-prod-$r --out ~/strata-tools/longctx/c5-prod-$r --tests speed --speed-sizes 1k,2k,4k,16k
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo C5DONE
