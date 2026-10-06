#!/usr/bin/env bash
# run the engine without the --layer-split the server adds for a multi-GPU config (helper / peer tests)
args=(); skip=0
for a in "$@"; do
  if [ $skip = 1 ]; then skip=0; continue; fi
  if [ "$a" = "--layer-split" ]; then skip=1; continue; fi
  args+=("$a")
done
exec "$(dirname "$0")/../../../engine/strata" "${args[@]}"
