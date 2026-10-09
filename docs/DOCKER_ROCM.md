# Strata in Docker on AMD GPUs (ROCm, Linux)

The fork's production setup (2x RX 7900 XTX, IQ3_XXS, 128K, the tuned pipeline and the conversation cache) as a
container: `Dockerfile.rocm`, `docker-compose.rocm.yml`, the settings in one `.env` file. Upstream's own `Dockerfile`
is for NVIDIA cards only.

**Status:** written and checked in pieces (the entrypoint's syntax, the settings overlay against the production
config, the build step against `setup.py`'s functions) on 09 Oct 2026; **the image has not been built or run yet**
(no Docker where it was written). The first build and start on the PC are the test - see "Checking it" below.

## Which ROCm

By default the image takes the ROCm `setup.py` pins for the cards (AMD's TheRock wheels; for RX 7900 cards
7.10.0a20251121, upstream's tested version). `ROCM_IMAGE` builds on an AMD ROCm image instead (`--build-arg
ROCM_IMAGE=rocm/dev-ubuntu-24.04:<tag>`, or `ROCM_IMAGE` in `.env` with the compose file's build args).

**ROCm 10.1 is not recommended on this PC** (measured 09 Oct, bench/results/2026-10-09-long-context-7900xtx): the
engine builds and runs with it, the answers and prompt reading are the same as with ROCm 7.9 and decode is ~3.5% slower
per window - but two full long-context runs with the conversation cache on each ended in a GPU memory fault right after
the cache parked a conversation (the engine restarted, one request failed); ROCm 7.9 ran the same twice with none. A
ROCm upgrade in place also needs `~/.cache/comgr` cleared (the old ROCm's cached kernels break the new one's start).

## What the image holds

- Ubuntu 24.04, the repository, its Python environment.
- ROCm (see above) and the engine compiled for `HIP_ARCHS` (default `gfx1100`), through `setup.py`'s own functions -
  the same path `setup.py` takes on a Linux PC. The host needs only the `amdgpu` kernel driver, no ROCm.
- Not the model: models, the prepared pack, the MTP layer and the configs are on the `/data` volume (a host
  folder). An existing `Strata-data` folder is reused as it is.

## Setting it up

1. Host: Docker with Compose, the `amdgpu` driver (`ls /dev/kfd /dev/dri/renderD*` lists the devices), 64 GB of RAM
   or more (Strata loads 40-60 GB into RAM).
2. The settings: `cp docker/rocm.env.example .env`, then edit `.env`:
   - `VIDEO_GID` / `RENDER_GID`: the numbers from `getent group video render` (the container needs them to open
     the GPU devices);
   - `STRATA_DATA_DIR`: the host folder for the data - the existing `Strata-data` folder to reuse the downloaded
     model, or a new folder (the first start then downloads ~45 GB for IQ3_XXS);
   - `API_KEY` before setting `BIND=0.0.0.0` (the network); with `BIND=127.0.0.1` only this PC reaches it.
3. Build (20-40 minutes once: ROCm's wheels and the engine):
   `docker compose -f docker-compose.rocm.yml build`
   (other cards: `HIP_ARCHS=gfx1201` in `.env` - see `Dockerfile.rocm`).
4. Start: `docker compose -f docker-compose.rocm.yml up -d`, then `docker compose -f docker-compose.rocm.yml logs -f`.
   The first start runs the setup (reusing or downloading the model, preparing its pack); later starts load the
   model in 1-3 minutes. It is ready when the log says `ready: http://...`.

## Using it

The same server as without Docker, on `PORT` (default 8080):

| | |
|---|---|
| web chat | `http://127.0.0.1:8080/` |
| OpenAI API | `http://127.0.0.1:8080/v1` - `/v1/chat/completions` (streaming), `/v1/responses`, `/v1/models` |
| Anthropic API | `http://127.0.0.1:8080/v1/messages` |
| status | `/health`, `/v1/status`, `/metrics` |

```
curl http://127.0.0.1:8080/v1/chat/completions -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" -d '{"model":"strata","messages":[{"role":"user","content":"Hello"}]}'
```

Any OpenAI client: `base_url="http://127.0.0.1:8080/v1"`, `api_key=<API_KEY>` (anything when none is set).

## The settings (`.env`)

Applied at every start by `docker/rocm_overlay.py`, so a change needs only
`docker compose -f docker-compose.rocm.yml up -d` (it recreates the container). An empty value keeps the config's
own; `off` removes an engine flag.

| variable | what | production value |
|---|---|---|
| `MODEL`, `CONTEXT`, `KV`, `GPUS` / `GPU`, `LAYER_SPLIT`, `KV_STREAMING` | the model and cards, used by the setup (first start, or once with `REINSTALL=1`) | `IQ3_XXS`, `131072`, `int8`, `1,0` |
| `HOST`, `PORT`, `API_KEY`, `BIND` | the server inside (`HOST`), the published port (`BIND:PORT`), the key | `0.0.0.0`, `8080`, -, `127.0.0.1` |
| `PIPELINE_WINDOWS`, `SPEC`, `SPEC_MIN_P`, `DRAFT_VOCAB` | one conversation's decode (FORK.md) | `2`, `3`, `0.7`, `en` |
| `VRAM_RESERVE_MIB`, `VRAM_RESERVE_LATER_MIB` | VRAM kept free on the first card / the later one (the desktop's card needs 3 GB) | `700`, `3072` |
| `KV_RESIDENT` | the attention cache's cells kept in VRAM (the rest streams from RAM) | `32768` |
| `CONVERSATION_CACHE_MIB`, `CONVERSATION_CACHE_SLOTS` | conversations parked in RAM (switching ~1 s instead of re-reading) | `16384`, `4` |
| `PARALLEL` | conversations decoded at once (8 for short-context agents; 1 for long contexts) | empty (1) |
| `STRATA_*` | the engine's switches, passed as they are | `STRATA_PIPELINE_DEBUG=1`, `STRATA_PIPELINE_THETA=0.1`, `STRATA_QFUSE=1` |
| `EXTRA_ARGS` | more engine flags, as on a command line | empty |
| `HIP_ARCHS` | the cards' architecture, at build time | `gfx1100` |

## Checking it

After the first start (the same checks as the fork's production tests, FORK.md):

1. `curl -s http://127.0.0.1:8080/v1/models` answers; `docker compose -f docker-compose.rocm.yml logs | grep -i
   "error\|fault"` shows nothing alarming; the log names both cards (`layer split: layers 0-25 (CUDA0), 26-47 (CUDA1)`).
2. The desktop card does not spill into system RAM (the cause of a 4x slowdown once): on the host,
   `cat /sys/class/drm/card*/device/mem_info_gtt_used` stays at a few hundred MB while the model answers.
3. Speed: a short chat decodes at ~90-110 tok/s, as outside Docker (the web chat shows it).
4. `curl -s http://127.0.0.1:8080/metrics` shows `"conversation_cache": {"enabled": true, ...}`.

## When something is wrong

- **"Permission denied" on `/dev/kfd` or no GPU found:** `VIDEO_GID` / `RENDER_GID` must be the host's numbers
  (`getent group video render`).
- **Pinned memory errors, or very slow loading:** the `memlock` limit (`ulimits` in the compose file) must stay
  unlimited.
- **Prompt reading slower than ~1,000 tok/s:** the hipBLASLt tuning table is chosen by the installed hipBLASLt's
  version (`tools/hip/gfx1100-hipblaslt-<version>.txt`); the engine log says when there is none for the ROCm the
  image installed, and the prompt's matrix products then use plain hipBLAS.
- **Decoding ~4x slower than expected:** the desktop card spills (check 2) - raise `VRAM_RESERVE_LATER_MIB`, or keep
  the card without the desktop first in `GPUS`.
- **A changed `MODEL` / `CONTEXT` / `KV` / `GPUS` is not taken:** set `REINSTALL=1` for one start.
