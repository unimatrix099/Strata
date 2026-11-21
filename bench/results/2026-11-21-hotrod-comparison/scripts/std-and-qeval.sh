#!/bin/bash
export LD_LIBRARY_PATH=/opt/rocm/lib
R=/workspace/compare-strata-vs-hotrod; H=/workspace/hotrod-audit/src; O=/workspace/Strata/terminal
PY=/workspace/Strata/.venv/bin/python
for spec in "ours:$O:$R/scripts/cfg-ours.json" "hotrod-matched:$H:$R/scripts/cfg-hotrod-matched.json" "hotrod-stock:$H:$R/scripts/cfg-hotrod-stock.json"; do
  IFS=: read name tree cfg <<< "$spec"
  echo "=== std $name $(date +%T)"
  cd $tree
  setsid nohup $PY $tree/serve/server.py --engine strata --config $cfg --port 8080 > $R/logs/$name-std-serve.log 2>&1 < /dev/null &
  for i in $(seq 1 120); do sleep 5; curl -s localhost:8080/health | grep -q '"loaded": true' && break; done
  cd $O
  $PY strata.py bench run --url http://127.0.0.1:8080 --no-start --skip-model-hash --track custom --track-notes "$name" --out $R/results/$name-standard 2>&1 | tail -25
  for p in $(ps -eo pid,stat,args | awk '$2!~/Z/ && /serve\/server.py --engine strata/ && !/awk/ {print $1}'); do kill $p; done; sleep 15
  echo "=== qeval $name $(date +%T)"
  QE_SERVER=$tree/serve/server.py $PY $R/scripts/qeval0217.py --label $name --config $cfg --set both --out $R/qeval 2>&1 | tail -3
  for p in $(ps -eo pid,stat,args | awk '$2!~/Z/ && /serve\/server.py --engine strata/ && !/awk/ {print $1}'); do kill $p; done; sleep 15
done
echo ALLDONE2
