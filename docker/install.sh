#!/usr/bin/env bash
# Strata on AMD GPUs with Docker - install and start with one command (docs/DOCKER_ROCM.md).
#
#   ./docker/install.sh                       # detect everything, ask before building, then build, start and wait
#   ./docker/install.sh --yes                 # no questions
#   ./docker/install.sh --data-dir /srv/Strata-data --api-key mykey --bind 0.0.0.0
#   ./docker/install.sh --model qwen:IQ3_XXS  # another model (family:size); --model swift:IQ3_S is the default on 96 GB+
#   ./docker/install.sh --env-only            # only write .env (detected values), build nothing
#
# What it detects: the AMD GPUs (from /sys/class/kfd: their architecture, VRAM, and which one drives a display - that
# one goes last, so the desktop's VRAM use does not squeeze the first stage), the host's video/render group IDs, the RAM
# (it picks a model that fits), and an existing Strata-data folder (reused: nothing is downloaded again). On 2 cards of
# 20 GB+ with gfx1100 (RX 7900 XT/XTX) it writes the tuned settings measured on 2x RX 7900 XTX (FORK.md); elsewhere it
# leaves those to setup's own choices. An existing .env is kept as .env.bak-<time>.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)

YES=0; ENV_ONLY=0; DATA_DIR=""; MODEL_ARG=""; PORT=8080; BIND=127.0.0.1; API_KEY=""; CONTEXT=262144
while [ $# -gt 0 ]; do
  case "$1" in
    --yes|-y) YES=1 ;;
    --env-only) ENV_ONLY=1 ;;
    --data-dir) DATA_DIR="$2"; shift ;;
    --model) MODEL_ARG="$2"; shift ;;
    --port) PORT="$2"; shift ;;
    --bind) BIND="$2"; shift ;;
    --api-key) API_KEY="$2"; shift ;;
    --context) CONTEXT="$2"; shift ;;
    -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
    *) echo "unknown option: $1 (--help)" >&2; exit 2 ;;
  esac
  shift
done

say() { printf '%s\n' "$*"; }
ok() { printf '  [ok] %s\n' "$*"; }
warn() { printf '  [!]  %s\n' "$*"; }
die() { printf '  [X]  %s\n' "$*" >&2; exit 1; }
ask() {   # ask "question" -> 0 for yes
  [ "$YES" = 1 ] && return 0
  printf '%s [Y/n] ' "$1"; read -r a || a=""
  case "$a" in n|N|no|NO) return 1 ;; *) return 0 ;; esac
}

say "Strata with Docker (AMD GPUs)"
say ""
say "== checking this PC"
# ---- Docker
if [ "$ENV_ONLY" = 0 ]; then
  command -v docker >/dev/null || die "docker is not installed (https://docs.docker.com/engine/install/)"
  docker compose version >/dev/null 2>&1 || die "the docker compose plugin is missing (docker compose version)"
  docker info >/dev/null 2>&1 || die "docker does not answer for this user: add yourself to the docker group, or run with sudo"
  ok "docker $(docker version --format '{{.Server.Version}}' 2>/dev/null || echo '?'), $(docker compose version --short 2>/dev/null || echo compose)"
fi
# ---- the AMD driver
[ -e /dev/kfd ] || die "/dev/kfd is missing: the amdgpu kernel driver is not loaded (an AMD GPU and a Linux kernel with amdgpu)"
[ -d /dev/dri ] || die "/dev/dri is missing"
ok "amdgpu driver: /dev/kfd and /dev/dri present"

# ---- the GPUs, in HIP's numbering (the KFD topology's GPU nodes, in order)
GPU_IDX=(); GPU_ARCH=(); GPU_VRAM=(); GPU_BUS=(); GPU_DISPLAY=()
i=0
for node in $(ls -d /sys/class/kfd/kfd/topology/nodes/* 2>/dev/null | sort -t/ -k8 -n); do
  props="$node/properties"
  [ -r "$props" ] || continue
  gv=$(awk '$1=="gfx_target_version"{print $2}' "$props")
  [ -n "$gv" ] && [ "$gv" != 0 ] || continue
  arch=$(printf 'gfx%d%x%x' $((gv / 10000)) $(((gv / 100) % 100)) $((gv % 100)))
  loc=$(awk '$1=="location_id"{print $2}' "$props"); dom=$(awk '$1=="domain"{print $2}' "$props")
  bus=$(printf '%04x:%02x:%02x.%x' "${dom:-0}" $((loc >> 8)) $(((loc >> 3) & 31)) $((loc & 7)))
  vram=0
  for mb in "$node"/mem_banks/*/properties; do
    [ -r "$mb" ] || continue
    s=$(awk '$1=="size_in_bytes"{print $2}' "$mb"); [ "${s:-0}" -gt "$vram" ] && vram=$s
  done
  disp=0
  for card in /sys/class/drm/card[0-9]*; do
    [ -e "$card/device" ] || continue
    [ "$(basename "$(readlink -f "$card/device")")" = "$bus" ] || continue
    n=$(cat "$card"-*/status 2>/dev/null | grep -c '^connected' || true); disp=$((disp + n))
  done
  GPU_IDX+=("$i"); GPU_ARCH+=("$arch"); GPU_VRAM+=($((vram / 1073741824))); GPU_BUS+=("$bus"); GPU_DISPLAY+=("$disp")
  i=$((i + 1))
done
NGPU=${#GPU_IDX[@]}
[ "$NGPU" -gt 0 ] || die "no AMD GPU found in /sys/class/kfd (is amdgpu loaded for the card?)"
for j in "${!GPU_IDX[@]}"; do
  ok "GPU ${GPU_IDX[$j]}: ${GPU_ARCH[$j]}, ${GPU_VRAM[$j]} GB VRAM, PCI ${GPU_BUS[$j]}$([ "${GPU_DISPLAY[$j]}" -gt 0 ] && echo ", drives ${GPU_DISPLAY[$j]} display(s)")"
done
ARCHS=$(printf '%s\n' "${GPU_ARCH[@]}" | sort -u | paste -sd';' -)
# the card order: the ones without a display first (they run the first layers and keep their whole VRAM for it)
GPUS=""; GPU1=""
if [ "$NGPU" -ge 2 ]; then
  first=(); last=()
  for j in "${!GPU_IDX[@]}"; do
    if [ "${GPU_DISPLAY[$j]}" -gt 0 ]; then last+=("${GPU_IDX[$j]}"); else first+=("${GPU_IDX[$j]}"); fi
  done
  GPUS=$(printf '%s\n' "${first[@]}" "${last[@]}" | grep -v '^$' | paste -sd, -)
  ok "the model's layers split across GPUs $GPUS (the display card last)"
else
  GPU1=0
fi

# ---- RAM and the model
RAM_GB=$(awk '/MemTotal/{printf "%d", $2/1048576}' /proc/meminfo)
if [ -n "$MODEL_ARG" ]; then
  FAMILY=${MODEL_ARG%%:*}; MODEL=${MODEL_ARG#*:}
  [ "$FAMILY" != "$MODEL_ARG" ] || { FAMILY=qwen; MODEL=$MODEL_ARG; }
elif [ "$RAM_GB" -ge 92 ]; then FAMILY=swift; MODEL=IQ3_S
elif [ "$RAM_GB" -ge 60 ]; then FAMILY=swift; MODEL=IQ3_XXS
elif [ "$RAM_GB" -ge 44 ]; then FAMILY=qwen; MODEL=IQ2_XS
else FAMILY=coder; MODEL=IQ1_M
fi
ok "RAM: $RAM_GB GB -> model $FAMILY $MODEL"

# ---- the host's groups for /dev/kfd and /dev/dri
gid_of() { getent group "$1" 2>/dev/null | cut -d: -f3 || true; }
VIDEO_GID=$(gid_of video); RENDER_GID=$(gid_of render)
[ -n "$VIDEO_GID" ] || VIDEO_GID=$(stat -c %g /dev/kfd)
if [ -z "$RENDER_GID" ]; then
  rn=$(ls /dev/dri/renderD* 2>/dev/null | head -1); RENDER_GID=$([ -n "$rn" ] && stat -c %g "$rn" || echo "$VIDEO_GID")
fi
ok "device groups: video $VIDEO_GID, render $RENDER_GID"

# ---- the data folder: an existing one is reused
if [ -z "$DATA_DIR" ]; then
  for c in "$ROOT/Strata-data" "$ROOT/../Strata-data" "$ROOT/strata-data"; do
    if [ -d "$c/models" ] || [ -d "$c/packs" ]; then DATA_DIR=$(cd "$c" && pwd); break; fi
  done
  [ -n "$DATA_DIR" ] || DATA_DIR="$ROOT/strata-data"
fi
mkdir -p "$DATA_DIR"
FREE_GB=$(df -BG --output=avail "$DATA_DIR" | tail -1 | tr -dc 0-9)
ok "data folder: $DATA_DIR ($FREE_GB GB free)"
existing=$(ls -d "$DATA_DIR"/models/* 2>/dev/null | xargs -r -n1 basename | paste -sd' ' -)
[ -n "$existing" ] && ok "models already there (reused): $existing"
case "$MODEL" in IQ3_S) need=90 ;; IQ3_XXS) need=82 ;; IQ2_XS) need=74 ;; *) need=72 ;; esac
[ "$FAMILY" = coder ] && need=66
tagdir=$([ "$FAMILY" = qwen ] && echo "$MODEL" || echo "$FAMILY-$MODEL")
if [ ! -d "$DATA_DIR/models/$tagdir" ] && [ "${FREE_GB:-0}" -lt "$need" ]; then
  warn "$FAMILY $MODEL needs about $need GB on $DATA_DIR (download + pack); $FREE_GB GB free"
fi

# ---- the tuned settings: measured on 2x RX 7900 XTX (gfx1100, 24 GB)
TUNED=0
if [ "$NGPU" = 2 ] && [ "$ARCHS" = gfx1100 ] && [ "${GPU_VRAM[0]}" -ge 20 ] && [ "${GPU_VRAM[1]}" -ge 20 ]; then
  TUNED=1; ok "2x gfx1100 with 20 GB+: the tuned settings of FORK.md (pipeline, drafting, the engine's switches)"
else
  ok "the engine's settings: setup's own choices for this PC (the tuned ones are for 2x RX 7900 XT/XTX)"
fi
if [ "$BIND" != 127.0.0.1 ] && [ -z "$API_KEY" ]; then
  die "--bind $BIND opens the server to the network: give it a key (--api-key KEY)"
fi

# ---- .env
say ""
say "== writing .env"
[ -f .env ] && cp .env ".env.bak-$(date +%Y%m%d-%H%M%S)" && ok "the previous .env kept as .env.bak-*"
tune() { [ "$TUNED" = 1 ] && printf '%s' "$1"; }
cat > .env <<EOF
# written by docker/install.sh on $(date '+%Y-%m-%d %H:%M') - every value: docker/rocm.env.example, docs/DOCKER_ROCM.md
VIDEO_GID=$VIDEO_GID
RENDER_GID=$RENDER_GID
STRATA_DATA_DIR=$DATA_DIR
BIND=$BIND
PORT=$PORT
API_KEY=$API_KEY
FAMILY=$FAMILY
MODEL=$MODEL
CONTEXT=$CONTEXT
KV=int8
GPUS=$GPUS
GPU=$GPU1
HIP_ARCHS=$ARCHS
ROCM_IMAGE=ubuntu:24.04
PIPELINE_WINDOWS=$(tune 2)
SPEC=$(tune 3)
SPEC_MIN_P=$(tune 0.7)
DRAFT_VOCAB=en
VRAM_RESERVE_MIB=$(tune 700)
VRAM_RESERVE_LATER_MIB=$(tune 3072)
KV_RESIDENT=$(tune 32768)
CONVERSATION_CACHE_MIB=16384
CONVERSATION_CACHE_SLOTS=4
PARALLEL=
FIT_MAX_TOKENS=1
TOOL_CALL_RECOVERY=1
STRATA_PIPELINE_DEBUG=$(tune 1)
STRATA_PIPELINE_THETA=$(tune 0.1)
STRATA_QFUSE=$(tune 1)
STRATA_PF_FUSED=$(tune 1)
STRATA_HIP_WMMA=$(tune 1)
STRATA_PREFILL_CPU_SHARE=$(tune 1)
STRATA_PREFILL_CPU_SHARE_MAX=$(tune 2048)
STRATA_PA_FAST=$(tune 1)
STRATA_HIP_POST_SPLIT=$(tune 1)
STRATA_HIP_FRONT_FUSE=$(tune 1)
EXTRA_ARGS=$( [ "$RAM_GB" -ge 92 ] && echo "--ple-io ram " )$(tune --remote-expert-opt)
EOF
ok ".env written"
[ "$ENV_ONLY" = 1 ] && { say ""; say "Done (--env-only). Start with: docker compose -f docker-compose.rocm.yml up -d --build"; exit 0; }

# ---- build and start
say ""
say "== building the image and starting Strata"
say "  The build compiles the engine for $ARCHS (10-20 minutes the first time). The first start then downloads"
say "  the model unless it is already in $DATA_DIR (an hour or more at 20 MB/s), prepares it and loads it."
ask "Build and start now?" || { say "Not started. Later: docker compose -f docker-compose.rocm.yml up -d --build"; exit 0; }
docker compose -f docker-compose.rocm.yml up -d --build

say ""
say "== waiting until the API answers (Ctrl-C stops waiting, not Strata; the log: docker compose -f docker-compose.rocm.yml logs -f)"
start=$(date +%s); last=""
until curl -fs "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; do
  state=$(docker inspect -f '{{.State.Status}}' strata 2>/dev/null || echo missing)
  [ "$state" = running ] || { docker compose -f docker-compose.rocm.yml logs --tail 40; die "the container is $state (the log above)"; }
  line=$(docker logs --tail 3 strata 2>&1 | tr '\r' '\n' | grep -v '^\s*$' | tail -1 | cut -c1-110)
  [ "$line" != "$last" ] && printf '  %s\n' "$line" && last=$line
  sleep 10
done
say ""
say "Strata is running ($(( ($(date +%s) - start) / 60 )) min)."
say "  Web chat:   http://$BIND:$PORT/"
say "  OpenAI:     http://$BIND:$PORT/v1      Anthropic: http://$BIND:$PORT/v1/messages"
say "  Test:       curl http://127.0.0.1:$PORT/v1/chat/completions -H 'Content-Type: application/json' \\"
say "                -d '{\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]}'"
say "  Log / stop: docker compose -f docker-compose.rocm.yml logs -f / down"
say "  Settings:   edit .env, then docker compose -f docker-compose.rocm.yml up -d (MODEL/CONTEXT/GPUS: REINSTALL=1 once)"
