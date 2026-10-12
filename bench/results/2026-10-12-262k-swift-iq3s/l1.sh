#!/usr/bin/env bash
# L1: the 262K context on Swift 1.5 IQ3_S (production config with --max-context 262144): speed, needles, facts, a long answer
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -f "engine/strata-pro[d]"
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
python3 ~/strata-tools/longctx.py --label l262 --out ~/strata-tools/longctx/l262 --config /workspace/strata-swift-iq3_s-tuned.json \
  --tests speed,needles,facts,longans --speed-sizes 128k,200k,240k --needle-sizes 128k,200k,240k \
  --fact-sizes 200k,240k --longans-sizes 240k
setsid nohup ./run-swift-iq3_s.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo L1DONE
