#!/usr/bin/env bash
# C3: the prompt path's CPU share - production's fixed 1 against auto and off, 1K / 2K / 4K prompts, two rounds
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
L=~/strata-tools/longctx.py
for r in 1 2; do
for v in 1 auto 0; do
python3 $L --label c3-$v-$r --out ~/strata-tools/longctx/c3-$v-$r --tests speed --speed-sizes 1k,2k,4k --env STRATA_PREFILL_CPU_SHARE=$v
done
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo C3DONE
