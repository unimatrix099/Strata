#!/usr/bin/env bash
# M1: IQ3_S (strata-iq3_s-tuned.json: the production config with IQ3_S's files) against IQ3_XXS (production)
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
B=~/strata-tools/bench.py
M=~/strata-tools/bench_many.py
L=~/strata-tools/longctx.py
S=/workspace/strata-iq3_s-tuned.json
python3 $B --label M1-s-first --reps 1 --config $S
for r in 1 2 3; do
python3 $M --label M1-xxs-$r --reps 1
python3 $M --label M1-s-$r --reps 1 --config $S
done
python3 $L --label m1-pf-xxs-1 --out ~/strata-tools/longctx/m1-pf-xxs-1 --tests speed --speed-sizes 1k,4k,16k,32k
python3 $L --label m1-pf-s-1 --out ~/strata-tools/longctx/m1-pf-s-1 --tests speed --speed-sizes 1k,4k,16k,32k --config $S
python3 $L --label m1-pf-s-2 --out ~/strata-tools/longctx/m1-pf-s-2 --tests speed --speed-sizes 1k,4k,16k,32k --config $S
python3 $L --label m1-pf-xxs-2 --out ~/strata-tools/longctx/m1-pf-xxs-2 --tests speed --speed-sizes 1k,4k,16k,32k
python3 ~/strata-tools/qeval.py --label iq3s --set easy --config $S
python3 ~/strata-tools/qeval.py --label iq3s --set hard --config $S
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo M1DONE
