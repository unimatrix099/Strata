#!/usr/bin/env bash
# O2: which fork default changes the text on one GPU: the fork's engine with STRATA_COMBINE_GATHER=0 (the old combine),
# with STRATA_MTP_PRIORITY=0 (the old drafter priority), and both, against the base's hashes (story f57d4a2173, code aaa15dc5b4)
cd /workspace
pkill -f "serve/serve[r].py"; sleep 3
B=~/strata-tools/bench.py
C=/workspace/strata-swift-iq3_s-1gpu.json
FORK="--exe /workspace/build-hip/strata"
EX="--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0"
python3 $B --label O2-nogather --reps 1 --config $C $EX $FORK --env STRATA_COMBINE_GATHER=0
python3 $B --label O2-noprio --reps 1 --config $C $EX $FORK --env STRATA_MTP_PRIORITY=0
python3 $B --label O2-both --reps 1 --config $C $EX $FORK --env STRATA_COMBINE_GATHER=0 --env STRATA_MTP_PRIORITY=0
python3 $B --label O2-fork-again --reps 1 --config $C $EX $FORK
echo O2DONE
