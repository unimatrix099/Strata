#!/usr/bin/env bash
# L2: the rest of the 262K check (L1 stopped after speed 128K / 200K): speed 240K, needles, facts, a long answer
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -f "engine/strata-pro[d]"
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 5
python3 ~/strata-tools/longctx.py --label l262b --out ~/strata-tools/longctx/l262b --config /workspace/strata-swift-iq3_s-tuned.json \
  --tests speed,needles,facts,longans --speed-sizes 240k --needle-sizes 128k,200k,240k \
  --fact-sizes 200k,240k --longans-sizes 240k
setsid nohup ./run-swift-iq3_s.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo L2DONE
