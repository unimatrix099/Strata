#!/usr/bin/env bash
# M2: Swift 1.5 IQ3_S (strata-swift-iq3_s-tuned.json: the production config with Swift's files) against IQ3_XXS
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3; pkill -x strata
while pgrep -f "serve/serve[r].py" > /dev/null; do sleep 2; done; sleep 10
B=~/strata-tools/bench.py
M=~/strata-tools/bench_many.py
L=~/strata-tools/longctx.py
S=/workspace/strata-swift-iq3_s-tuned.json
python3 $B --label M2-sw-first --reps 1 --config $S
for r in 1 2 3; do
python3 $M --label M2-xxs-$r --reps 1
python3 $M --label M2-sw-$r --reps 1 --config $S
done
python3 $L --label m2-pf-sw-1 --out ~/strata-tools/longctx/m2-pf-sw-1 --tests speed --speed-sizes 1k,4k,16k,32k --config $S
python3 $L --label m2-pf-sw-2 --out ~/strata-tools/longctx/m2-pf-sw-2 --tests speed --speed-sizes 1k,4k,16k,32k --config $S
python3 ~/strata-tools/qeval.py --label swift-iq3s --set easy --config $S
python3 ~/strata-tools/qeval.py --label swift-iq3s --set hard --config $S
setsid nohup ./run-iq3_xxs.sh > /workspace/server-prod.txt 2>&1 < /dev/null &
echo M2DONE
