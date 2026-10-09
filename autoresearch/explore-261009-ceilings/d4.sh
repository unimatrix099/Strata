#!/usr/bin/env bash
# D4: the first draft's own floor (STRATA_SPEC_MIN_P0, build-hip/strata): 0 (one draft always), 0.3, 0.5 against
# the unchanged rule (0.7 for every draft) on the same binary; pipelined, 2 reps. Runs after D3.
until grep -q D3DONE ~/strata-tools/d3.out; do sleep 15; done
cd /workspace
pkill -f "serve/serve[r].py" ; sleep 8
B=~/strata-tools/bench.py
X="--exe /workspace/build-hip/strata"
python3 $B --label D4-base --reps 2 $X
python3 $B --label D4-p0-0 --reps 2 $X --env STRATA_SPEC_MIN_P0=0
python3 $B --label D4-p0-03 --reps 2 $X --env STRATA_SPEC_MIN_P0=0.3
python3 $B --label D4-p0-05 --reps 2 $X --env STRATA_SPEC_MIN_P0=0.5
python3 $B --label D4-base-2 --reps 2 $X
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo D4DONE
