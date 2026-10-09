#!/usr/bin/env bash
# D1: the drafter's numbers - acceptance by depth (STRATA_SPEC_DEPTH) and what a second branch at the first rejected
# depth would have caught (STRATA_MTP_TOP2, slow diagnostic); spec 3 (production) and spec 4
cd /workspace
pkill -f "serve/server.py --engine strata --config /workspace/strata-iq3_xxs.json" ; sleep 8
B=~/strata-tools/bench.py
python3 $B --label D1-depth-spec3 --reps 1 --env STRATA_SPEC_DEPTH=1
python3 $B --label D1-top2-spec3 --reps 1 --env STRATA_SPEC_DEPTH=1 --env STRATA_MTP_TOP2=1
python3 $B --label D1-depth-spec4 --reps 1 --env STRATA_SPEC_DEPTH=1 --drop-arg=--spec --arg=--spec --arg=4
python3 $B --label D1-top2-spec4 --reps 1 --env STRATA_SPEC_DEPTH=1 --env STRATA_MTP_TOP2=1 --drop-arg=--spec --arg=--spec --arg=4
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D1DONE
