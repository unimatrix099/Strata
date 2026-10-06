# Two RX 7900 XTX in one PC: what was measured and changed (2026-10-05)

One day of work on a PC with two AMD RX 7900 XTX: installing Strata, then making the two cards faster, first for one
conversation and then for several at once (coding agents and their sub-agents). Every number below was measured on
this PC; the details, raw results and tools are in the two result folders linked in each section.

| | before | after |
|---|---|---|
| one conversation, both cards (decode) | 55.0 tok/s (one card alone: 60) | **72.2 tok/s** |
| 4 conversations at once (in all) | 73.4 tok/s, the 4th answer starts after 21 s | **101-105 tok/s**, after 2.8-3.0 s |
| 8 conversations at once (in all) | 70.9 tok/s, the 8th answer starts after 51 s | **150-151 tok/s**, after 5.2-5.8 s |
| prompts (~4.6K tokens) | 1,245 tok/s | 1,241-1,246 tok/s (unchanged) |
| **on upstream's new main (07 Oct), one conversation, `--pipeline-windows 2`** | 58.0 tok/s (upstream as it was) | **92.7 tok/s** |
| **on upstream's new main, 8 conversations at once** | 70.9 tok/s | **185.8 tok/s** |

The answers are the same: greedy runs give the same tokens as before (checked by hashes and by the repository's
exactness tests).

## The PC

- 2x AMD Radeon RX 7900 XTX (gfx1100, 24 GB each), both on PCIe 3.0 x8 (the engine's probe: 7.1 GB/s). HIP device 0
  (PCI 0a) also drives two monitors.
- Ryzen 9 5950X (16 cores, AVX2, no AVX-512), 126 GB RAM; Linux in a container without root; system ROCm 7.9.0
  (hipBLASLt 1.1.0).
- Model: Qwen3.8-Flash-Next IQ3_XXS (3-bit), 128K context, 8-bit KV with 32K resident, MTP drafts.

## 1. Installing (docs/AMD_HIP.md, "Two cards, one of them the desktop's")

- **Build on ROCm 7.9:** its clang rejects `__shared__ alignas(16)`; `iq_kernels.cu` uses `__shared__ __align__(16)`
  like the other kernels (commit `c6d4458`).
- **The container had no `python3-venv`:** the setup's `.venv` was made with `get-pip.py` (no root needed).
- **The desktop card:** with it as the first card and the default 700 MiB VRAM reserve, amdgpu moved 21 GB of the
  engine's memory to system RAM (GTT) while the engine's log said everything fit: 80 tok/s prompts, 15 tok/s decode.
  `--vram-reserve-mib 3072` fixed it (later measured: either card order then decodes the same). Only
  `/sys/class/drm/card*/device/mem_info_gtt_used` shows such a spill.

## 2. One conversation on both cards (bench/results/2026-10-05-split-decode-7900xtx)

A layer split runs layers 0-25 on one card and 26-47 on the other. A token needs layer 25's result before layer 26
can start, so the cards take turns; the work was to shorten the time between the turns.

- **Where the time went:** per decode window (~2 tokens, 39.6 ms) 16.1 + 14.0 ms of GPU work on the two cards, and
  the rest in the hand-off: stage 1's graph launch (1.25-1.5 ms of CPU for ~1,200 kernels on ROCm) only after stage 0
  had finished, synchronous commits, the draft. The split was slower than one card alone (54-55 vs 60 tok/s).
- **Kept:** the split's commits no longer wait (55.0 -> 57.5), and stage 1's window graph is launched early by a
  worker thread and waits on a flag until stage 0's result is there (57.5 -> 72.2). Part of the gain is clocks: RDNA3
  lowers the shader clock in idle gaps (1.5 GHz); the waiting card now spins and stays at ~2.95 GHz.
- **Dropped:** keeping stage 0's card busy with a spin kernel (no gain), HIP graph runtime switches, other speculation
  settings, another split point, the default VRAM reserve (the desktop card spilled again).
- **Safety:** an independent review found one bug in the early launch's error path (a failed window could leave the
  other card spinning); fixed and tested with the #267 stall hook.
- Switches: `STRATA_SPLIT_COMMIT_SYNC=1`, `STRATA_SPLIT_EARLY_LAUNCH=0`; one card never reaches this code.

## 3. Several conversations at once (bench/results/2026-10-05-multi-agent-7900xtx)

The engine already had batch slots (`--batch N`: one token of each conversation per window) and a pipeline for a
layer split (`--batch-groups G`: card A runs one group while card B runs another). On this PC it barely helped:

- **Pad rows:** a group always ran all its slots; an idle slot cost a dummy token's experts on every card. With 8
  slots and 2 requests: 39 tok/s in all, below one request at a time. Now a group runs only its active slots
  (70 tok/s), the tokens identical to each request alone.
- **No pipeline from the config:** `"parallel": N` gave only `--batch N` (all slots in one window, card after card).
  The server now adds `--batch-groups` on several GPUs (`"batch_groups": 1` turns it off): 97 -> 150 tok/s at 8.
- **Unlucky slot choice:** two requests in one group ran card after card (26 instead of 38 tok/s each); a new request
  now goes to the group with the fewest running ones.
- Pipelined slots keep their conversation cache again (an agent's next turn does not read its history again), and
  setup suggests `--parallel` on several cards where the slots fit.

Use: `"parallel": 8` in `strata-<model>.json` when two or more requests often run at once. Costs: a request alone is
~5% slower (the slots' VRAM leaves 75% instead of 83% of the experts in VRAM), and a request sharing the cards decodes
at 20-40 tok/s instead of ~78 (slots do not use MTP drafts).

## 4. Can the experts of one conversation run on both cards at the same time?

The model is a mixture of experts, so the question was whether one conversation's experts could be split across the
cards so that neither waits. What the profile says (`STRATA_VERIFY_PROFILE=1`, 4-row windows, per card and window):

| | stage 0 | stage 1 |
|---|---|---|
| GPU time per window | 18.4 ms | 17.9 ms |
| routed experts (VRAM hits, PCIe groups, copy + combine) | 3.9 ms | 3.4 ms |
| everything else (attention, recurrent layers, router, shared expert, head) | 14.5 ms | 14.5 ms |

- The layer split already spreads the experts: each card holds the experts of its own layers.
- Splitting each layer's experts between the cards (expert parallelism) could save at most half the routed experts'
  time, ~2 ms of an 18 ms window, and every layer would need two cross-card hand-offs (48 layers, each waiting on the
  other card); the rest of the layer - attention, the recurrent state, the router, the shared expert - cannot be split
  that way. Not worth it here; not built.
- Splitting every matrix across the cards (tensor parallelism) helped dense models a lot in llama.cpp on this same
  PC, but it would mean a new window graph in this engine and an all-reduce per layer over PCIe 3.0 x8. Not tried.
- **Measured on 2026-10-06** (bench/results/2026-10-06-expert-parallel-7900xtx): Strata's two expert-parallel modes
  run on AMD but are slower than the layer split (72.9 tok/s): the peer tier (`--peer-device`) 67.2, the helper cache
  (`--expert-cache-device1`) 53.8-59.3 (and 18.6 with `auto`, which filled the desktop card). Both meet at every layer
  through the host: 8.6-10.5 ms of every window. The cards themselves meet in 2.8-11.7 us when they signal each other
  directly (a 4-64 KB block into the other card's VRAM), ~0.6 ms per window for 48 layers. But measured before
  building it, the peer tier signalled card to card would reach only ~69-78 tok/s: the main card's own work per
  window (attention, recurrent state, projections, router, shared expert, head) is ~21 ms, the same as the split's
  two cards in turn, and the experts it hands off are ~2-3 ms of it. Not worth building. A tensor split's bound from
  a kernel trace is +15-40% (weeks of work); fusing the ~2,400 small kernels of a window helps one card and the split
  alike.

So for one conversation the cards take turns today, and the work went into making the turns shorter (section 2);
with several conversations both cards work at the same time (section 3). Expert parallelism does not beat the split
for one conversation on this model, even signalled card to card, because the experts are a small part of a token's
work; only a tensor split (weeks, +15-40% estimated) would put both cards on every part of it.

## 5. Fewer kernels per token (bench/results/2026-10-06-kernel-fusion-7900xtx)

A kernel in a HIP graph costs at least 2.86 us on these cards, and a decode window launches ~2,435 of them, two
thirds under 8 us; about a third of a layer's GPU time is the space between kernels. Merging the CPU's expert rows,
the GPU's and the combine into one kernel kept the same tokens and saved ~0.34 ms of a window (~1%,
`STRATA_COMBINE_GATHER=0` turns it off). Merging the indexer's appends saved nothing: they sit mostly in the commit,
which runs while that card is idle. Only kernels on the window's path from layer to layer count; the ceiling with
many more merges is estimated at ~5%.

## 6. On upstream's new main (bench/results/2026-10-07-upstream-port-7900xtx)

Upstream rewrote its `main` on 2026-10-06 (1,159 new commits). Branch `amd-7900xtx-port` carries our changes onto it.
Upstream as it was decoded 58.0 tok/s here; with our asynchronous commit and early launch 77.5; with its own new
`--pipeline-windows 2` - card 1 starts the next window on a guess while card 2 verifies this one, rolled back when the
guess is wrong - 92.7 tok/s (+25% over the same build without it). Eight conversations at once: 185.8 tok/s (151 on
the old branch). The early launch adds nothing on top of the pipeline but is +29% without it; the merged combine is
~0.5% there. The stream-fork switch and `k8v4` gave nothing.

## 7. Next steps, not done

- **A VRAM reserve per card:** only the desktop card needs 3 GB; the other could hold ~2.3 GB more experts. The CPU's
  experts cost 2.9-4.4 ms per window with 8 slots (75% in VRAM) and 0.7-1.6 ms for one conversation.
- **MTP drafts in batch slots:** a conversation in a slot decodes one token per window (20-40 tok/s) against ~2 with
  drafts alone (~78 tok/s); the largest lever for a request that shares the cards.
- **Prelaunching the pipeline's graphs** (as the split does for one conversation): small here, the launches are 0.9 /
  0.34 ms of a ~21 ms window with 8 conversations.
- **The engine could report the pipeline groups it runs** (the server assumes the ones it asked for).

## Checks and tools

- Tests: ctest 61 of 65 on this PC, the same four failures before and after (a Q2_0 fixture, AVX-512, the
  container's memlock limit, a gfx1012-only signed-zero case); `tools/batch_test.py` and
  `tools/batch_interleave_test.py` identical to solo; `serve/test_parallel.py`, `serve/test_server.py`,
  `tools/test_setup_parallel.py` pass. `tools/test_setup_amd.py` (1) and `tools/test_setup_golden.py` (46) fail on
  this PC with and without these changes; the last step of `batch_interleave_test.py` (a prompt giving way) fails
  with the base engine too.
- Tools: `bench.py` / `show.py` / `smoke.py` (one conversation: speed, hashes, server checks) and `mbench.py`
  (several clients at once) in the two result folders; `vpy.py` runs a repository script with the setup's Python.
  Each folder's `iterations.tsv` lists every try in order with its numbers and the decision.
- Raw engine and server logs stayed on the PC (`data/logs/` is ignored by git); `data/results.jsonl` holds every
  measured run.
