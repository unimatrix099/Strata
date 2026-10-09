#!/usr/bin/env bash
cd /workspace
until grep -q PFDONE /workspace/autoresearch/explore-261009-ceilings/run_pf.txt; do sleep 10; done
python3 ~/strata-tools/bench.py --label VPROF --env STRATA_VERIFY_PROFILE=1 --env STRATA_DECODE_TIMING=1 --env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0 --reps 2 --out ~/strata-tools/data/results.jsonl
echo VPDONE
