#!/usr/bin/env bash
# D3: the first draft's gate. Production gates every draft at p >= 0.7, so about half the windows carry no draft;
# variants: one draft always (spec 2, min-p 0), spec 3 at min-p 0.4 and 0.0, against the installed config. Pipelined, 2 reps.
cd /workspace
pkill -f "serve/serve[r].py" ; sleep 8
B=~/strata-tools/bench.py
S="--drop-arg=--spec --drop-arg=--spec-min-p"
python3 $B --label D3-base --reps 2
python3 $B --label D3-spec2-mp0 --reps 2 $S --arg=--spec --arg=2 --arg=--spec-min-p --arg=0
python3 $B --label D3-spec3-mp04 --reps 2 $S --arg=--spec --arg=3 --arg=--spec-min-p --arg=0.4
python3 $B --label D3-spec3-mp0 --reps 2 $S --arg=--spec --arg=3 --arg=--spec-min-p --arg=0
python3 $B --label D3-base-2 --reps 2
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D3DONE
