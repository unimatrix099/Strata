#!/usr/bin/env bash
# C4: does the armed CPU share cost long prompts too? share 1 (production) against off, 4K / 16K / 32K, two rounds
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
L=~/strata-tools/longctx.py
for r in 1 2; do
for v in 1 0; do
python3 $L --label c4-$v-$r --out ~/strata-tools/longctx/c4-$v-$r --tests speed --speed-sizes 4k,16k,32k --env STRATA_PREFILL_CPU_SHARE=$v
done
done
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo C4DONE
