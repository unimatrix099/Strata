# Several agents at once on a two-GPU split: 2x RX 7900 XTX (2026-10-05)

**Question:** with a layer split one card waits while the other works. Can both work when several requests (coding
agents, sub-agents) run at once?

**Answer:** yes, through the pipelined batch slots (`--batch N --batch-groups 2`): card A runs one group of
conversations while card B runs another. After three fixes here, `"parallel": 8` on this pair:

| requests at once | one at a time (default) | `"parallel": 8` | the last answer's first token |
|---|---|---|---|
| 1 | 72.0 tok/s | 66-69 tok/s | 0.6 s / 0.6 s |
| 2 | 69.7 | 69.5 | 8.5 s / 1.4 s |
| 4 | 73.4 | 105.4 (+44%) | 21.4 s / 2.8 s |
| 8 | 70.9 | 150.5 (+112%) | 51.1 s / 5.8 s |

(total tok/s of all requests; one at a time / parallel in the last column). A request alone is ~5% slower: the 8
slots' sessions take VRAM from the expert cache (75% instead of 83% of the experts resident).

## Rig, model, method

The same as bench/results/2026-10-05-split-decode-7900xtx (2x RX 7900 XTX, PCIe 3.0 x8, Ryzen 9 5950X, 126 GB,
ROCm 7.9.0, IQ3_XXS, 128K context, int8 KV with 32K resident, MTP, `--gpus 1,0 --vram-reserve-mib 3072`), on top of
that branch's split changes.

`mbench.py` starts the server with a variant of the installed config (engine flags, config keys such as
`--set parallel=8`), then for 1, 2, 4 or 8 clients runs rounds of streamed requests started together (different
agent-style tasks, greedy, 512 tokens, thinking on), and records the total tok/s (all tokens / time until the last
answer ended), each request's tok/s and its time to the first token. Rounds use the task list in order, so compare
runs with the same client list. `vpy.py` runs a script with the setup's Python (the repo's tests need its packages).

## What was measured and changed

**1. Pipelined slots help only with full groups (before the fixes):**

| config (split at 26, `--trim-stage-weights`) | 2 clients | 4 clients | 8 clients |
|---|---|---|---|
| one at a time | 69.7 | 73.4 | 70.9 |
| `--batch 2 --batch-groups 2` | 74.1 | 76.2 | - |
| `--batch 4 --batch-groups 2` | 57.6 | **112.0** | - |
| `--batch 4 --batch-groups 4` | 71.9 | 72.4 | 77.4 |
| `--batch 8 --batch-groups 2` | **39.0** | 74.1 | **148.1** |
| `--batch 8 --batch-groups 4` | 50.7 | 55.4 | 109.3 |

A request in a slot decodes one token per window (no MTP drafts): 20-40 tok/s each, so the slots win only when there
are enough of them running.

**2. No pad rows (`b01fefb`).** A pipelined group always ran all its slots; an idle slot was a pad row (token 0)
that cost a token's routed experts on every stage. Now a group's step runs its active slots only (fixed at stage 0
for every stage of the step). `tools/batch_test.py` (`--pcie-frac 0 --adapt-every 1000000`): 3 of 8 and 8 of 8 slots
IDENTICAL to their solo tokens.

| | 2 clients | 4 clients | 8 clients |
|---|---|---|---|
| `--batch 8 --batch-groups 2` before | 39.0 | 74.1 | 148.1 |
| after | **70.1** | **104.8** | 150.8 |
| `--batch 4 --batch-groups 2` before | 57.6 | 112.0 | - |
| after | **75.4** | 112.1 | - |

**3. The server pipelines `"parallel"` on a split and spreads requests over the groups (`c5674a0`).** `"parallel": N`
gave only `--batch N` (all slots in one window, card after card). On several GPUs it now also gives
`--batch-groups` (`"batch_groups": 1` turns it off):

| `--batch 8`, auto split | 2 clients | 4 clients | 8 clients |
|---|---|---|---|
| without groups | 48.7 | 70.0 | 97.0 |
| 2 groups | 70.9 | 104.6 | 150.0 |

A new request now goes to the pipeline group with the fewest running requests: two in one group run card after
card (one round measured 26-27 tok/s each instead of 37-39); three rounds after the change: 68.7, 70.9, 69.5 tok/s.
`--trim-stage-weights` with an explicit split gave the same numbers as the auto split.

**4. Setup's note (`3cffee9`).** On several cards setup said "one at a time - costs 10-25% speed per request" (a 12 GB
card's measurement). It now suggests `--parallel 8` with the numbers above; nothing is written unless asked.

**5. Review fixes.** An independent review of 2-4 found no correctness bug and three risks, fixed: a pipelined slot
is kept as a conversation cache again (dropping it was only needed for the pad rows; `tools/batch_interleave_test.py
--batch 4` with `--batch-groups 2`: "slot 0 gave back 144 tokens of this conversation", the next turn IDENTICAL to
solo); setup suggests the most slots (8, 6, 4 or 2) whose sessions fit a fifth of the cards' expert cache, and
promises the pipeline only when the server adds groups; a string `"batch_groups"` is said. Left as is: each mix of
active slots is captured once as a graph of its own (a one-time pause); the engine does not report the groups it
runs, so a split skipped at start (`--split-skip-if-fits`) keeps the server's groups (harmless: one window).

## Tried and dropped

- **Graph launches on a worker thread** (the pump thread serves both stages' layers): 105.4 vs 108-112 tok/s at 4
  clients. The cards already overlap: 1,022 group-steps in 16.4 s = 16 ms per stage window, so the one thread is not
  the limit.
- **Prelaunching the next group's graphs** (as the solo split does): not built. With 8 clients the launches are 0.9
  and 0.34 ms per stage window of ~21 ms (`STRATA_SPLIT_TIMING`); the CPU's experts are 2.9 and 4.4 ms (75% resident).

## Where the time goes now (8 clients, 2 groups of 4)

188 rows/s while decoding; each stage window (4 rows) ~21 ms: GPU work, then 2.9 ms (stage 0) and 4.4 ms (stage 1)
of CPU experts per window (the 5950X has no AVX-512), launches 0.9 / 0.34 ms. More VRAM for the expert cache is the
next lever (a per-card reserve: only the desktop card needs 3 GB).

## Checks

- `tools/batch_test.py`: identical tokens, partial and full groups (above).
- `tools/batch_interleave_test.py --batch 4` pipelined: A, B and the long C IDENTICAL to solo, A's next turn from
  its slot IDENTICAL; its last step (a prompt giving way) stops with "no such free slot" here, with the base engine
  as well.
- `tools/early_close_test.py` against `"parallel": 8`: solo OK, batch OK.
- 6 streamed requests at once, 3 left by their clients after 2 s, then 4 new ones: every finished answer right
  (144, a haiku, Blue, 15, Rome, No, 1024), no errors in the engine log.
- `serve/test_parallel.py` 13/13 (new: groups for 1-3 GPUs and the config key; the slot choice), `serve/test_server.py`
  139/139, `tools/test_setup_parallel.py` 4/4; `test_setup_amd` (1) and `test_setup_golden` (46) fail on this
  machine with and without these changes.

## Use it

```
"parallel": 8
```

in `strata-<model>.json` (or `./setup.sh --parallel 8`), then restart. Worth it when two or more requests often run
at once (an agent with sub-agents, several users); for one user typing one prompt at a time leave it out.

## Reproduce

```sh
python3 bench/results/2026-10-05-multi-agent-7900xtx/mbench.py --label solo --clients 1,2,4,8
python3 bench/results/2026-10-05-multi-agent-7900xtx/mbench.py --label p8 --clients 1,2,4,8 --set parallel=8
python3 bench/results/2026-10-05-multi-agent-7900xtx/vpy.py tools/batch_test.py --exe engine/strata \
    --config strata-iq3_xxs.json --batch 8 --n 8 --extra "--layer-split 26 --batch-groups 2 --pcie-frac 0 --adapt-every 1000000"
```

`data/results.jsonl` holds every run (engine and server logs stayed on the PC: git ignores `logs/`);
`iterations.tsv` lists every try in order with its decision. Overview of the day's work: docs/RESEARCH_2X_7900XTX.md.
