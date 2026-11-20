#!/bin/sh
# Entry point of the ROCm image (Dockerfile.rocm, docs/DOCKER_ROCM.md).
#
# First start (no config on /data for MODEL, or REINSTALL=1): setup.py sets the model up on the /data volume -
# downloading it, or reusing the files already there - without starting it. Every start: docker/rocm_overlay.py
# writes the env vars' settings into that config (so changing one needs only a restart), then setup.py starts the
# server (OpenAI- and Anthropic-compatible API on PORT).
set -e
cd /opt/strata || exit 1

STRATA_DATA="${STRATA_DATA:-/data}"
FAMILY="${FAMILY:-qwen}"
MODEL="${MODEL:-IQ3_XXS}"
CONTEXT="${CONTEXT:-131072}"
HOST="${HOST:-0.0.0.0}"         # inside the container; which host address the port is published on is compose's
PORT="${PORT:-8080}"
API_KEY="${API_KEY:-}"
KV="${KV:-}"                    # int8 | q4_0 | k8v4; empty: setup.py's default (int8)
GPUS="${GPUS:-}"                # "1,0": both cards, the first one listed runs the first layers (docs/AMD_HIP.md)
GPU="${GPU:-}"                  # one card
LAYER_SPLIT="${LAYER_SPLIT:-}"
LOW_RAM="${LOW_RAM:-auto}"
GGUF_DIR="${GGUF_DIR:-}"
KV_STREAMING="${KV_STREAMING:-}"
CONFIG="${CONFIG:-}"

case "$FAMILY" in qwen) prefix="" ;; *) prefix="${FAMILY}-" ;; esac
tag="${prefix}$(printf '%s' "$MODEL" | tr 'A-Z' 'a-z')"
cfg="${CONFIG:-$STRATA_DATA/config/strata-$tag.json}"
mkdir -p "$STRATA_DATA/config"

if [ -z "$CONFIG" ] && { [ "${REINSTALL:-0}" = "1" ] || [ ! -f "$cfg" ]; }; then
  echo "Setting up $tag on $STRATA_DATA (files already there are reused; otherwise the model is downloaded)."
  set -- --family "$FAMILY" --model "$MODEL" --context "$CONTEXT" --data-dir "$STRATA_DATA" \
    --host "$HOST" --port "$PORT" --api-key "$API_KEY" --no-start --low-ram "$LOW_RAM" --vision no
  [ -n "$KV" ] && set -- "$@" --kv "$KV"
  [ -n "$GPUS" ] && set -- "$@" --gpus "$GPUS"
  [ -n "$GPU" ] && set -- "$@" --gpu "$GPU"
  [ -n "$LAYER_SPLIT" ] && set -- "$@" --layer-split "$LAYER_SPLIT"
  [ -n "$GGUF_DIR" ] && set -- "$@" --gguf-dir "$GGUF_DIR"
  [ -n "$KV_STREAMING" ] && set -- "$@" --kv-streaming "$KV_STREAMING"
  .venv/bin/python setup.py --setup --yes "$@"
  [ -e "/opt/strata/strata-$tag.json" ] && ! [ -L "/opt/strata/strata-$tag.json" ] && cp -f "/opt/strata/strata-$tag.json" "$cfg"
fi
[ -f "$cfg" ] || { echo "No config at $cfg: the setup did not finish (see above)." >&2; exit 1; }

# the copy on the volume is the one that counts; setup.py starts the strata-*.json in /opt/strata
ln -sfn "$cfg" "/opt/strata/strata-$tag.json"
.venv/bin/python docker/rocm_overlay.py "$cfg"
echo "Config: $cfg"

set -- --port "$PORT"
[ -n "$GPUS" ] && set -- "$@" --gpus "$GPUS"
[ -n "$GPU" ] && set -- "$@" --gpu "$GPU"
[ -n "$LAYER_SPLIT" ] && set -- "$@" --layer-split "$LAYER_SPLIT"
exec .venv/bin/python setup.py "$@"
