#!/bin/bash
# runeng.sh LABEL TREE CFG : start Strata server from TREE/(terminal/)serve/server.py with CFG on :8080, run standard-v1 (old-tree runner), greedy.py, pf.py, stop
LABEL=$1; TREE=$2; CFG=$3
S=/tmp/claude-1000/-workspace/3bbb7e51-c7da-48fb-9c25-6468453dedfe/scratchpad; R=/workspace/compare-strata-0217
PY=/workspace/Strata/.venv/bin/python
SRV=$TREE/serve/server.py; [ -f $SRV ] || SRV=$TREE/terminal/serve/server.py
cd $(dirname $(dirname $SRV))
setsid nohup $PY $SRV --engine strata --config $CFG --port 8080 > $S/$LABEL-serve.log 2>&1 < /dev/null &
for i in $(seq 1 90); do sleep 5; curl -s localhost:8080/health | grep -q '"loaded": true' && break; done
curl -s localhost:8080/health | grep -q '"loaded": true' || { echo "NOT LOADED"; tail -20 $S/$LABEL-serve.log; exit 1; }
grep -E "expert cache [0-9]+ slots|layer split auto" $S/*-engine.log 2>/dev/null | tail -2 | cut -c1-200
cd /workspace/Strata
$PY strata.py bench run --url http://127.0.0.1:8080 --no-start --skip-model-hash --track custom --track-notes "$LABEL" --out $R/results/$LABEL-standard 2>&1 | grep -E "^  (short|prompt)|Sanity|Status"
python3 $S/greedy.py 8080 $LABEL 2>&1 | grep -E "DECODE|PREFILL"
for p in $(ps -eo pid,stat,args | awk '$2!~/Z/ && (/serve\/server.py --engine strata/ || /--serve --pack/) && !/awk/ {print $1}'); do kill $p; done; sleep 12
