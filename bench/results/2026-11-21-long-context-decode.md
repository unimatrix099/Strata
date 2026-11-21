# Long-context decode on production (prod-1121), 21 Nov 2026

Question: what is the maximum context and the decode speed at that size?

- Max context: **262,144 tokens** (model maximum, `--max-context 262144`; prompt + answer share the window, `fit_max_tokens` shortens answers).
- Engine: `engine/strata-prod-1121`, config `strata-swift-iq3_s-tuned.json` (2x RX 7900 XTX, `--gpus 1,0`, `--kv int8`, `--kv-resident 32768`, `--prefill 12288`, `--spec 3 --mtp`, `--pipeline-windows 2`).
- Script: `scripts/longdec.py <port>` in `2026-11-21-hotrod-comparison/scripts/` (synthetic `record%06d: alpha beta ...` lines, then a 400-word story request; max_tokens 500, temperature 0, cache_prompt false).

| Context | Prompt read | Time to first token | Decode | Drafts accepted |
|---|---|---|---|---|
| ~127K | 3,814 tok/s | 33 s | 113.4 tok/s | 268/310 |
| ~242K | 3,465 tok/s | 70 s | 104.1 tok/s | 240/280 |

Short-prompt decode is ~117 tok/s, so a nearly full window costs about 10%.

12 Oct reference (older engine, same box): prompt 3,036/2,846/2,824 tok/s and decode 84/75/76 tok/s at 128K/200K/240K; needles 9/9, facts 2/2.

Caveats: synthetic repetitive text and greedy decoding (may inflate draft acceptance), single run per size, needle/fact quality not re-run on this engine, nothing tested beyond 262K or with several long conversations at once.
