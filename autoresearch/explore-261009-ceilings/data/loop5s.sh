#!/usr/bin/env bash
# step 5: device-planned layers under the pipeline (STRATA_VERIFY_DEVICE_PLAN=1 STRATA_PL_DEVICE_PLAN=1), the new build:
# exact protocol first (the tier fixed: the text must be the same, the window shorter), then the installed config
cd /workspace
until grep -q S4DONE ~/strata-tools/loop4s.txt; do sleep 15; done
[ -f ~/strata-tools/build-devplan.ok ] || { echo "no build"; echo S5DONE; exit 0; }
cp build-hip/strata ~/strata-tools/strata-devplan
B=~/strata-tools/bench.py; O=~/strata-tools/data/results.jsonl
X=(--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0 --reps 1 --env STRATA_DECODE_TIMING=1 --env STRATA_VERIFY_PROFILE=1 --exe /home/node/strata-tools/strata-devplan --out $O)
python3 $B --label S5-exact-base "${X[@]}"
python3 $B --label S5-exact-dp "${X[@]}" --env STRATA_VERIFY_DEVICE_PLAN=1 --env STRATA_PL_DEVICE_PLAN=1
python3 $B --label S5-exact-dp2 "${X[@]}" --env STRATA_VERIFY_DEVICE_PLAN=1 --env STRATA_PL_DEVICE_PLAN=1
python3 $B --label S5-exact-base2 "${X[@]}"
M=~/strata-tools/bench_many.py
python3 $M --label S5-cfg-dp --reps 1 --exe /home/node/strata-tools/strata-devplan --env STRATA_VERIFY_DEVICE_PLAN=1 --env STRATA_PL_DEVICE_PLAN=1 --out $O
python3 $M --label S5-cfg-base --reps 1 --exe /home/node/strata-tools/strata-devplan --out $O
echo S5DONE
