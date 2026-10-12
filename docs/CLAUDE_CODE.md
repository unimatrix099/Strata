# Claude Code on the local Strata server

Strata answers Anthropic's Messages API (`POST /v1/messages`: streaming, tool use, thinking blocks, images), so
Claude Code can use the local model as its provider. Measured on 2x RX 7900 XTX with Swift 1.5 IQ3_S, Claude Code
2.1.296, 11 Oct 2026.

## In short: what was already there, and what this fork added

**The Anthropic-compatible API is upstream Strata's** (Claude Code support since 0.1.17): `POST /v1/messages` with
streaming, tool use, thinking blocks and images; the prompt cache that pins Claude Code's per-request billing stamp so
its long system prompt is read once and reused; and the server keys for agents (`fit_max_tokens`,
`tool_call_recovery`). Upstream's docs gave the connection variables in one line.

**Added here on 11 Oct:**
1. The setup tested end to end: three real Claude Code sessions on the production model (Swift 1.5 IQ3_S), every
   result checked by running its tests by hand (below).
2. The settings that matter, checked against the installed Claude Code - above all `CLAUDE_CODE_MAX_CONTEXT_TOKENS`
   set to the server's context, so Claude Code compacts the conversation before the server would refuse it.
3. `tools/strata-claude.sh`: starts Claude Code with the local server for that session only; a plain `claude` keeps
   using Anthropic.
4. `fit_max_tokens` and `tool_call_recovery` on in the production config.
5. This guide (linked from the README and FORK.md).

## Start it

```
tools/strata-claude.sh                 # interactive, in the current folder
tools/strata-claude.sh -p "run the tests and fix what fails"
```

The wrapper asks the running server for its model name and context window and sets everything below for that one
session; a plain `claude` keeps using Anthropic's API. Another machine, or a server with an API key:
`STRATA_URL=http://<host>:8080 STRATA_API_KEY=<key> tools/strata-claude.sh`.

Without the wrapper, the same as env vars (or in the `"env"` block of a Claude Code `settings.json` - user
`~/.claude/settings.json` applies to every session, a project's `.claude/settings.local.json` to that folder only):

| variable | value | why |
|---|---|---|
| `ANTHROPIC_BASE_URL` | `http://127.0.0.1:8080` | the server; Claude Code adds `/v1/messages` itself |
| `ANTHROPIC_AUTH_TOKEN` | the server's `api_key`, or any text when it has none | sent as `Authorization: Bearer` |
| `ANTHROPIC_MODEL`, `ANTHROPIC_DEFAULT_OPUS_MODEL`, `_SONNET_MODEL`, `_HAIKU_MODEL` | the model's name (`/v1/models`), e.g. `swift-1.5-iq3_s` | Strata serves any name; Claude Code only logs `unrecognized_model` |
| `CLAUDE_CODE_MAX_CONTEXT_TOKENS` | the server's context (262144 here since 12 Oct; 131072 before) | Claude Code assumes a Claude window otherwise; it says so itself ("set CLAUDE_CODE_MAX_CONTEXT_TOKENS to its real window") |
| `CLAUDE_CODE_AUTO_COMPACT_WINDOW` | ~3/4 of it (196608 at 262K) | compacts the conversation before the server would refuse it |
| `CLAUDE_CODE_MAX_OUTPUT_TOKENS` | 16000 | its default asks for 32,000 on every request |
| `API_TIMEOUT_MS` | 1800000 | a long first prompt is read before the first token |
| `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC`, `DISABLE_TELEMETRY`, `DISABLE_ERROR_REPORTING` | 1 | nothing goes to Anthropic |

(Checked against the installed Claude Code: the percentage variant of the compaction setting is
`CLAUDE_AUTOCOMPACT_PCT_OVERRIDE`; `CLAUDE_CODE_AUTOCOMPACT_PCT_OVERRIDE` does not exist.)

On the server, two keys in `strata-<model>.json` suit agents (both on in the production config since 11 Oct):
`"fit_max_tokens": true` (a long conversation plus the client's output cap is shortened to the room left instead of
refused) and `"tool_call_recovery": true` (tool calls the model writes in another form are still read as calls: 98.3%
read as intended against 92.9% without, on 1,462 Claude Code turns - DETAILS.md).

## What was measured

Three headless sessions (`claude -p`, all tools allowed) in a scratch project, each checked by running its tests by
hand afterwards:

| task | turns | time | result |
|---|---|---|---|
| fix two bugs in `stats.py` (an off-by-one mean, a median wrong for even lengths) so `test_stats.py` passes | 6 | 22 s | correct |
| add `variance()` and `stdev()` with `ValueError` on empty input, and their tests | 8 | 22 s | correct |
| add a `wc`-style command line to `wc.py` (aligned columns, a total line) and a `test_wc.py` using subprocess | 15 | 60 s | correct (4 tests, a missing-file case included) |

The server's side: Claude Code's system prompt and tool list are ~26,700 tokens. The first request of the first
session read them in 13 s; every later turn reused 97-99% of the prompt and started in 0.3-1 s; a new session reused
the cached system prompt (24,211 of 26,711 tokens) and started in 3.8 s. Answers and tool calls were written at
100-155 tok/s; Swift 1.5 thinks 23-168 tokens per agent turn. Every tool call (Bash, Read, Edit, Write) was read
cleanly.

## Limits

- **One request at a time.** Background agents and parallel subagents wait their turn; `"parallel": N` in the
  model's config serves several at once, at the cost of VRAM for experts (docs/BATCHING.md).
- **Server-side tools** (web search) do not exist locally; Claude Code's local tools (Bash, Read, Edit, Grep, Glob,
  WebFetch) work.
- **The context** is the server's (262,144 here since 12 Oct), not a Claude model's 200K or 1M: keep
  `CLAUDE_CODE_MAX_CONTEXT_TOKENS` at the real value so compaction comes in time.
- A local model is not Claude: on these small, well-specified tasks it was correct every time; harder, longer agent
  work is where the difference shows.
