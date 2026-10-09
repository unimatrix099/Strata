# Long context on 2x RX 7900 XTX: quality and speed up to 128K (2026-10-09)

Can the fork's production build be trusted with long prompts and long answers - no garbage, no drift into random
text after some token count? Tested up to the configured limit, 128K tokens (131,072), through the normal server.

**The PC and build:** 2x RX 7900 XTX (PCIe 3.0 x8, one drives the desktop), Ryzen 9 5950X, 125 GB RAM, ROCm 7.9;
the fork's `main` at `4c64fe9` (upstream `fb58e0d` + the fork's changes), installed as `engine/strata`; the installed
config `strata-iq3_xxs.json` unchanged: IQ3_XXS, `--max-context 131072`, `--kv int8 --kv-resident 32768` (the
attention cache beyond 32K streams from RAM), `--pipeline-windows 2 --spec 3 --spec-min-p 0.7`, the English draft
vocabulary, `STRATA_QFUSE=1`. Greedy unless said otherwise; thinking off unless said otherwise.

**How:** `longctx.py` (this folder) starts the server once and runs six tests; every answer is checked for
degeneration (a stretch that compresses below 0.12 - a loop -, an 8-word sequence repeated more than 8 times, a
unit repeated 5+ times in a row at the end, broken characters, CJK text in an English answer), and the flagged ones
were read. The text is the repository's own docs and source (`tools/needle_bench.py`'s haystack), a different stretch
per test so the prompt cache does not help. Raw rows in `data/results.jsonl`, the table in `data/report.md`, every
answer in `data/answers/`.

## Result

**No garbage and no drift at any length.** Every recall and reasoning check passed up to 127K tokens; long answers
after 122K of prompt stayed coherent to their end; 9K-16K generated tokens stayed coherent; no engine error, no GTT
spill (the desktop card stayed at its idle 30 / 100 MiB).

### Recall: a code word at 10 / 50 / 90% of the prompt (`tools/needle_bench.py`)

| prompt | 10% | 50% | 90% |
|---|---|---|---|
| 8K | found | found | found |
| 32K (33.5K) | found | found | found |
| 64K (63.8K) | found | found | found |
| 128K (127.0K) | found | found | found |

**12 of 12.**

### Reasoning over facts spread through the prompt (thinking on)

Three notes at 20 / 55 / 85% of the prompt; the question needs two of them combined (vault BETA's code = ALPHA's code
plus a number) and a third recalled (a courier's name).

| prompt | code | courier | answer |
|---|---|---|---|
| 33.6K | right (7264) | right | `1. Vault BETA access code: **7264** 2. Vault GAMMA courier: ...` |
| 63.3K | right (7700) | right | |
| 118.6K | right (4631) | right | |

**3 of 3.**

### A long answer after a long prompt (a ~3000-word report on the files in the prompt, streamed)

| prompt | first token after | answer | decode tok/s (by quarter of the answer) | end | checks |
|---|---|---|---|---|---|
| 32.2K | 27.7 s | 2,683 tokens | 101.3 (104.8 / 105.9 / 100.2 / 95.3) | finished | clean |
| 66.7K | 58.4 s | 2,803 tokens | 94.9 (95.0 / 94.6 / 95.2 / 95.2) | finished | clean |
| 121.7K | 98.7 s | 2,960 tokens | 92.2 (88.5 / 91.3 / 91.7 / 98.4) | finished | clean |

The answers end at 124.7K tokens of context in all, coherent reports with headings to their last line (read).

### Very long generation from a short prompt (16,000 tokens allowed)

| task | tokens | tok/s (by quarter) | end | checks |
|---|---|---|---|---|
| story, greedy | 9,055 | 101.3 (93.1 / 99.3 / 101.2 / 113.7) | finished | clean |
| story, temperature 0.7 | 11,073 | 105.1 (87.7 / 90.5 / 117.5 / 140.8) | finished | flagged, read: fine (below) |
| program + tests, greedy | 16,000 | 152.8 (147.1 / 155.3 / 155.3 / 154.0) | the limit | flagged, read: fine (below) |
| program + tests, temperature 0.7 | 14,469 | 139.7 (133.1 / 141.2 / 140.3 / 144.8) | finished | clean |

Both flags were the check, not the text: the sampled story repeats a deliberate refrain 11 times over 11K tokens
("He wrote in his journal. He described the arrival of the boat... / the storm, the distress call... / the cave,
the bones..."), each with its own content, and ends coherently; the program repeats a database guard opening each of
24 methods and uses `# =====` section lines (the loop check took a divider for a loop; it now ignores them), and is
valid code up to the token limit, in the middle of a unit test. The style of a 3-bit model's long story is somewhat
repetitive; nothing drifted.

### A conversation grown to 96K tokens (the prompt cache's path)

Four parts of ~24K tokens each, every one with a project codename hidden at a random place, then "list the
codenames of parts 1-4 in order":

| turn | conversation | read this turn | first token after |
|---|---|---|---|
| part 1 | 23.3K | 23.3K | 17.2 s |
| part 2 | 48.2K | 25.0K | 19.7 s |
| part 3 | 72.1K | 23.9K | 18.0 s |
| part 4 | 96.2K | 24.0K | 23.1 s |
| recall | 96.2K | 40 tokens | - |

All four codenames, in order. Each turn read only its new part (the earlier ones came from the cache).

### Speed by prompt length (a 3-sentence summary of a unique prompt)

| prompt | reading | first token after | decode |
|---|---|---|---|
| 4.2K | 915 tok/s | 4.6 s | 88.6 tok/s |
| 15.1K | 1,100 tok/s | 13.7 s | 100.3 tok/s |
| 29.9K | 1,097 tok/s | 27.2 s | 94.6 tok/s |
| 61.9K | 1,205 tok/s | 51.4 s | 91.5 tok/s |
| 120.9K | 1,119 tok/s | 108.1 s | 73.7 tok/s |

Prompt reading holds ~1,100-1,200 tok/s to 121K. Decode slows little with context: 92-101 tok/s in the long answers
up to 122K (the 73.7 here is a 112-token answer, where the pipeline's start weighs more). Beyond 32K the attention
cache streams from RAM: at 121K 92-96% of its block reads still hit VRAM (115-220 MiB read from RAM per answer).

## Several conversations at once, 64K each (`"parallel"`)

`longctx.py --tests par --set parallel=N --par-n N`: N conversations started together, each with its own ~64K prompt
holding four notes (its own codes and courier), asking for vault BETA's code (ALPHA's code plus a number), vault
GAMMA's courier and a ~1000-word summary (thinking off, greedy), then a second turn asking for vault DELTA's shelf
number. Checked: the answers, no other conversation's code or courier in any answer, degeneration. The same two
conversations were also run one at a time (`parallel` 1) for comparison. Data in `data/par2-64k`, `data/par4-64k`,
`data/solo2-64k`.

| | conversations right (courier, follow-up) | BETA code | other conversations' facts in an answer | first tokens after | all done |
|---|---|---|---|---|---|
| 2 at once | 2 of 2 | 1 of 2 | none | 56 / 113 s | 146 s |
| 4 at once | 4 of 4 | 3 of 4 | none | 53 / 113 / 175 / 230 s | 402 s |
| the same 2, one at a time | 2 of 2 | 1 of 2 | none | 54 / 120 s | 135 s |

- **Correct and separate.** Every conversation found its own facts; no answer contained another's; no
  degeneration, no engine error, no GTT spill. The one wrong BETA code is the same conversation every time, alone too
  (4115 + 4113 given as 8128 instead of 8228, both numbers quoted right): the model's arithmetic without thinking,
  not the slots.
- **Slower than one at a time at 64K.** The prompts are read one after another either way (~55-65 s each), and while
  four slots each hold ~64K a batch window carries one token per conversation (no drafts in slots) over a long
  attention: the engine counted 5,731 tokens in 343 s for the four (16.7 tok/s in all, the prompt reads included),
  against ~100 tok/s for one conversation alone. Two at once finished in 146 s against 135 s one at a time.
- **Follow-up turns:** a slot keeps its conversation, so a follow-up read only its 32-34 new tokens (0.3-1.1 s) - for
  2 of 4 at once; the other two had finished early, were moved back to the single path and re-read their 64K history
  (60-67 s). One at a time, the single prompt cache holds one conversation, so both follow-ups re-read 64K (49-53 s).

So for long contexts, `"parallel"` keeps several users' conversations correct and apart, but does not add speed on
this PC; it pays with short contexts (agents: 180 tok/s in all at 8). For people alternating between long
conversations, the conversation cache (`--conversation-cache-mib`, docs/DETAILS.md) is the thing to try - not
measured here.

## Switching between long conversations: the conversation cache (`--conversation-cache-mib`)

`longctx.py --tests convcache`: three conversations of ~64K (four notes each), then three rounds of follow-up
questions alternating between them (A, B, C, A, ...) - the single prompt cache's worst case. Without the cache, then
with `--conversation-cache-mib 16384 --conversation-cache-slots 4` added to the engine's arguments (upstream's,
off by default). Data in `data/cc-off`, `data/cc-on` (with the engine's park / restore lines).

| | follow-up re-reads | first token after a switch | answers right |
|---|---|---|---|
| without | the whole conversation, 63-67K tokens | 50.3-58.3 s | 9 of 9 |
| with the conversation cache | 31-33 tokens (the new question) | 0.73-1.29 s | 9 of 9, the same answers |

A parked 64K conversation is a 1.5-2.4 GB snapshot in RAM (the attention cache, the recurrent state, the
checkpoints, the draft layer's cache); parking took 0.3-0.8 s (later parks reuse the unchanged K/V pages: 0.96-1.02
GB of them), restoring 0.17-0.20 s. The three conversations held 4.1-5.8 GB; the lowest free RAM was 70.4 GiB against
74.1 without. A 16 GB budget holds about 6-7 conversations of 64K (3-4 of 128K); the oldest is evicted first.

## ROCm 10.1 against ROCm 7.9 (09 Oct)

ROCm 10.1 (released 2026-10-05; it lists the RX 7900 XTX) has no wheels or apt packages for these cards yet; AMD ships
it as the Docker image `rocm/dev-ubuntu-24.04:10.1.0-full`. Its ROCm folder was unpacked without Docker
(`data/rocm/pull_rocm.py`), the engine built against it with setup.py's flags (`build_rocm.py`; hipBLASLt 1.4.1, so the
`gfx1100-hipblaslt-100401` table applies) and run through `strata-r10.sh` (its libraries first). Driver: amdgpu
6.19.14 / 31.40 with kernel 6.8 (ROCm 10.1's own `rocminfo` and HIP tests ran). Data in `data/rocm/`.

| | ROCm 10.1 | ROCm 7.9 (installed) |
|---|---|---|
| eight prompts, the installed config (two runs each): tok/s / ms per window | 104.8, 108.2 / 17.53, 17.59 | 108.6, 107.7 / 16.92, 17.03 |
| a 4.6K-token prompt read | 1,146 / 1,145 tok/s | 1,148 / 1,098 tok/s |
| prompt reading at 118K, the same suite with the conversation cache | 1,211 tok/s | 1,200 tok/s |
| `smoke.py` | 7 of 7 | 7 of 7 |
| full long-context suite with the conversation cache, twice | **a GPU memory fault in each run** (the engine restarted; one request failed with 503) | no fault in either |
| the long-generation tests alone, without the cache | no fault | - |

Both faults came right after the conversation cache parked a conversation (4 parked, 8-10 GB, evictions):
"Memory access fault by GPU node-1 ... Page not present" at a host address. The answers were right in every test
that ran (needles, facts, 96K conversation); one greedy story looped at its end on ROCm 10.1 once and not when
repeated. A first try failed at start for another reason: `~/.cache/comgr` held kernels compiled by ROCm 7.9, which
ROCm 10.1's runtime reused (`undefined hidden symbol: __amd_streamOpsIncrement`); clearing the cache fixed it.

So: ROCm 10.1 is no faster here and not safe with the conversation cache; production stays on ROCm 7.9, and the
Docker image defaults to setup.py's pinned ROCm 7 (docs/DOCKER_ROCM.md). (An earlier reading of "+16-19% prompt reading
at long context" for 10.1 was the conversation cache's run against one without it, not ROCm: like for like they are
the same.)

## Not tested

Contexts above 128K (the config's limit), more than 4 long conversations at once, other languages than
English, images, and a reference model's output for comparison (the checks are recall, arithmetic, degeneration and
reading the flagged answers).

## Run it again

```
python bench/results/2026-10-09-long-context-7900xtx/longctx.py --label <name> --out <dir>
# a variant: --exe <engine>, --arg=<flag> / --drop-arg=<flag> / --env K=V; a subset: --tests needles,facts
```

About 20 minutes on this PC.
