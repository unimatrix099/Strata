#!/usr/bin/env bash
cd /workspace
B=bench/results/2026-10-05-split-decode-7900xtx/bench.py
O=autoresearch/explore-261007-tg/results.jsonl
X=(--env STRATA_IQ_MT_MIN=1 --arg=--pcie-frac --arg=0 --arg=--adapt-every --arg=0 --out "$O" --reps 1)
python3 $B --label SC-new-serial-ours-off "${X[@]}" --exe /workspace/build-hip/strata --drop-arg=--pipeline-windows --env STRATA_SPLIT_COMMIT_SYNC=1 --env STRATA_SPLIT_EARLY_LAUNCH=0
python3 $B --label SC-old-serial "${X[@]}" --drop-arg=--pipeline-windows
python3 $B --label SC-old-pw2 "${X[@]}"
