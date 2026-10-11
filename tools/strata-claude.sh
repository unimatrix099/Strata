#!/usr/bin/env bash
# Claude Code on the local Strata server (docs/CLAUDE_CODE.md): the settings for this one session only, so a plain
# `claude` keeps using Anthropic's API. Every argument goes to claude.
#
#   tools/strata-claude.sh                       # interactive, in the current folder
#   tools/strata-claude.sh -p "fix the failing test and run the tests"
#   STRATA_URL=http://192.168.1.20:8080 STRATA_API_KEY=mykey tools/strata-claude.sh
#
# It asks the server for its model name and context window, and sets: the base URL, a token (the server's key, or any
# text when it has none), that model for every tier, the real context window (so auto-compact starts before the
# server would refuse a request), an output cap that leaves room in the context, a long timeout for prompt reading,
# and no non-essential traffic to Anthropic (telemetry, error reports, update checks).
set -euo pipefail
STRATA_URL="${STRATA_URL:-http://127.0.0.1:8080}"
STRATA_URL="${STRATA_URL%/}"; STRATA_URL="${STRATA_URL%/v1}"

command -v claude >/dev/null || { echo "claude (Claude Code) is not installed: https://code.claude.com/docs" >&2; exit 1; }
auth=(-H "Authorization: Bearer ${STRATA_API_KEY:-local}")
models=$(curl -fs -m 10 "${auth[@]}" "$STRATA_URL/v1/models") \
  || { echo "Strata does not answer at $STRATA_URL (is it running? the API key: STRATA_API_KEY)" >&2; exit 1; }
model=$(printf '%s' "$models" | python3 -c 'import sys, json; print(json.load(sys.stdin)["data"][0]["id"])')
ctx=$(curl -fs -m 10 "${auth[@]}" "$STRATA_URL/metrics" 2>/dev/null \
      | python3 -c 'import sys, json; e = json.load(sys.stdin)["engine"]; print(e.get("context") or e.get("max_context") or "")' \
      2>/dev/null || true)
ctx=${ctx:-131072}

export ANTHROPIC_BASE_URL="$STRATA_URL"
export ANTHROPIC_AUTH_TOKEN="${STRATA_API_KEY:-local}"
unset ANTHROPIC_API_KEY CLAUDE_CODE_OAUTH_TOKEN
export ANTHROPIC_MODEL="$model" ANTHROPIC_DEFAULT_OPUS_MODEL="$model" ANTHROPIC_DEFAULT_SONNET_MODEL="$model"
export ANTHROPIC_DEFAULT_HAIKU_MODEL="$model"
export CLAUDE_CODE_MAX_CONTEXT_TOKENS="$ctx"
export CLAUDE_CODE_AUTO_COMPACT_WINDOW="${STRATA_COMPACT_AT:-$(( ctx * 3 / 4 ))}"
export CLAUDE_CODE_MAX_OUTPUT_TOKENS="${STRATA_MAX_OUTPUT:-16000}"
export API_TIMEOUT_MS="${API_TIMEOUT_MS:-1800000}"
export CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1 DISABLE_TELEMETRY=1 DISABLE_ERROR_REPORTING=1
echo "Claude Code -> Strata at $STRATA_URL (model $model, context $ctx, auto-compact at $CLAUDE_CODE_AUTO_COMPACT_WINDOW)" >&2
exec claude "$@"
