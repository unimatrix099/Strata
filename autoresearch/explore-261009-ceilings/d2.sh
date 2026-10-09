#!/usr/bin/env bash
# D2: the drafter's numbers without the pipeline (the per-depth and top2 counters live in the serial loop only):
# acceptance by depth (STRATA_SPEC_DEPTH) and the runner-up's hits at the first rejected depth (STRATA_MTP_TOP2); spec 3 and 4
cd /workspace
B=~/strata-tools/bench.py
P="--drop-arg=--pipeline-windows"
python3 $B --label D2-depth-spec3 --reps 1 --env STRATA_SPEC_DEPTH=1 $P
python3 $B --label D2-top2-spec3 --reps 1 --env STRATA_SPEC_DEPTH=1 --env STRATA_MTP_TOP2=1 $P
python3 $B --label D2-depth-spec4 --reps 1 --env STRATA_SPEC_DEPTH=1 $P --drop-arg=--spec --arg=--spec --arg=4
python3 $B --label D2-top2-spec4 --reps 1 --env STRATA_SPEC_DEPTH=1 --env STRATA_MTP_TOP2=1 $P --drop-arg=--spec --arg=--spec --arg=4
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D2DONE
