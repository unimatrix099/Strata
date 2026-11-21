#!/bin/bash
export LD_LIBRARY_PATH=/opt/rocm/lib
R=/workspace/compare-strata-vs-hotrod; H=/workspace/hotrod-audit/src
for round in A B; do
  for spec in "ours:/workspace/Strata/terminal:$R/scripts/cfg-ours.json" "hotrod-matched:$H:$R/scripts/cfg-hotrod-matched.json" "hotrod-stock:$H:$R/scripts/cfg-hotrod-stock.json"; do
    IFS=: read name tree cfg <<< "$spec"
    echo "=== $name-$round $(date +%T)"; $R/scripts/runeng-hotrod.sh $name-$round $tree $cfg
  done
done
echo ALLDONE
