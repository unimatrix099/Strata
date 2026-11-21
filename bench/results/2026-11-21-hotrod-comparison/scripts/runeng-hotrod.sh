#!/bin/bash
# Same protocol as /workspace/compare-strata-0217/scripts/runeng.sh, for the HotRod tree.
# runeng-hotrod.sh LABEL TREE CFG : start server from TREE/serve/server.py with CFG on :8080, standard-v1 + greedy.py, stop.
# Run it twice per engine (fresh server each time). TODO when GPUs are back: write cfg-hotrod-*.json (see README.md).
LABEL=$1; TREE=$2; CFG=$3
R=/workspace/compare-strata-vs-hotrod; S=$R/logs; mkdir -p $S
PY=/workspace/Strata/.venv/bin/python
cd $TREE
setsid nohup $PY $TREE/serve/server.py --engine strata --config $CFG --port 8080 > $S/$LABEL-serve.log 2>&1 < /dev/null &
for i in $(seq 1 120); do sleep 5; curl -s localhost:8080/health | grep -q '"loaded": true' && break; done
curl -s localhost:8080/health | grep -q '"loaded": true' || { echo "NOT LOADED"; tail -20 $S/$LABEL-serve.log; exit 1; }
cd /workspace/Strata   # the SAME bench runner for both engines (frozen fixtures)
$PY strata.py bench run --url http://127.0.0.1:8080 --no-start --skip-model-hash --track custom --track-notes "$LABEL" --out $R/results/$LABEL-standard 2>&1 | grep -E "^  (short|prompt)|Sanity|Status"
python3 $R/scripts/greedy.py 8080 $LABEL 2>&1 | grep -E "DECODE|PREFILL"
for p in $(ps -eo pid,stat,args | awk '$2!~/Z/ && /serve\/server.py --engine strata/ && !/awk/ {print $1}'); do kill $p; done; sleep 12
