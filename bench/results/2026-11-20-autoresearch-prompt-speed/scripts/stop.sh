#!/bin/bash
# stop.sh PORT : stop the server on PORT and its engine
P=$1
for p in $(ps -eo pid,stat,args | awk -v p="--port $P" '$2!~/Z/ && /serve\/server.py --engine strata/ && index($0,p) && !/awk/ {print $1}'); do
  for c in $(ps -o pid= --ppid $p); do kill $c 2>/dev/null; done; kill $p; done
for i in $(seq 1 20); do sleep 1; ps -eo args | grep -q "serve[r].py --engine strata.*--port $P" || break; done; sleep 3
