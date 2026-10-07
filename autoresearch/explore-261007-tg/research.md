# Token generation on 2x RX 7900 XTX - research round (07 Oct 2026)

Goal: one conversation's decode speed, beyond what was already tried. Baseline (exact protocol, `--pipeline-windows 2
--spec 3 --spec-min-p 0.7`, theta 0.1, en draft vocab, `STRATA_QFUSE=1`): ~95.0 tok/s, ~18.4 ms per window
(winms.py), story 3e41c3 / code 32e166.

## Measured this round

| idea | result |
|---|---|
| VRAM reserve, exact protocol (`--pcie-frac 0 --adapt-every 0`), 8 prompts (bench_many.py) | 3072/3072: 99.8 / 99.9 tok/s, 18.65 ms/window, hit 82-94%; 2048/2048: 105.5 / 104.4, 17.5 ms; 700 (card 1) / 3072 (desktop card, `--vram-reserve-later-mib`): 107.2 / 107.3, 17.59 ms; 700/2048: 106.1 / 106.5, 17.3 ms; 1536/1536 (2 prompts): slower than 2048. GTT stayed at idle (30 / 448 MiB) in every run |
| the same in the installed config (adaptive tier, PCIe share on) | old 101.6 / 104.3 tok/s, 17.36 / 17.14 ms/window, hit 97.9-99.0%; 700/3072: 107.2 / 105.7, 17.39 / 17.33 ms, hit 98.4-99.5% - the same windows (the tok/s gap is the text: 1.76-1.79 vs 1.83-1.86 tokens per window). The PCIe share already serves the misses; the larger cache only pays when misses go to the CPU. Config now `--vram-reserve-mib 700 --vram-reserve-later-mib 3072` (no loss, a little more cache) |
| later layer split (card 2 also runs the draft chain) | 26: 18.37 ms/window; 28: 18.45-18.69; 30: 19.5; 32: 21.9 (hit rate 87.7 -> 75.7%). The text changes with the split (a different CPU/GPU expert mix), so tok/s is not comparable; ms per window says keep 26 |

## From the literature (what is new for Strata)

| idea | source | already in Strata? | fit here |
|---|---|---|---|
| asynchronous draft / verify on separate devices | AMUSD (arXiv 2410.17375), PEARL (2408.11850), PipeSpec (2505.01572) | yes: upstream `--pipeline-windows 2` | - |
| tree verification for Gated DeltaNet hybrids (several draft branches per window, one accepted state rebuilt) | TreeWY (2608.20961), STree (2505.14969) | no (chains only) | lossless; raises tokens per window; large work (GDN replay per branch) |
| windowed draft attention | Windowed-MTP (2607.21535) | yes (the draft layer's 16K window) | - |
| megakernel / persistent kernel per layer | MPK (OSDI'26), Hazy Research "No Bubbles" | no | card 2: ~1/3 of time is gaps between ~938 kernels per window; very large work |
| hybrid CPU/GPU expert scheduling, score caching, prefetch | HybriMoE (2504.05897), Fiddler, kTransformers | yes (profile cache, CPU misses, pcie share, adaptive tier, lookahead) | - |
| expert prefetch from draft tokens | DraftExpert (2607.24434), MoE-SpeQ (2511.14102) | partly (lookahead warms pages) | the drafter is one layer: no per-layer routing to predict from |
| MTP head fine-tuned on self-distilled data (acceptance 2nd/3rd token 11->56%, 2->36%) | FastMTP (2509.18362) | no | a new draft model: set aside by the user |
| ROCm graph dispatch improvements | ROCm 7.2.4 notes | our ROCm 7.9 (TheRock) is newer | - |

## Where the pipeline's guesses fail (STRATA_PIPELINE_TRACE, 251 windows, older settings spec 4 / min-p 0.5)

| window K's verdict | share |
|---|---|
| all drafts accepted, bonus guessed right (card 1's speculative window kept) | 31.9% |
| all drafts accepted, the bonus token guessed wrong | 36.7% |
| fewer drafts accepted | 31.4% |

More than half of the wasted speculative windows fail on one token: the target's bonus after the drafts, which card 1
takes from the drafter. A second branch with the drafter's second choice would rescue the share of those where it is
right.

Today's config (8 prompts, 2,066 windows): bonus right 48.8%, bonus wrong 37.9%, fewer accepted 13.3% - the bonus is
74% of the wasted speculative windows.

**The drafter's second choice** (`STRATA_MTP_TOP2=1` with `STRATA_DECODE_TIMING=1`, a diagnostic: a kernel keeps each
chain row's second-best id, the pipeline counts it at the verdict): of 757 whole windows whose bonus guess was wrong,
the second choice was the target's token in 236 (31%). A fresh window after a miss took ~22.3-23.9 ms, a kept
speculative one ~10.9-12.6 ms (`strata pipeline classes`); 236 x ~11.5 ms = 2.7 s of the run's 35.0 s (2,018 windows):
**at most ~8%** from a second branch on card 1 (B with the second choice as its row 0), before what that branch costs
card 1 (more rows, a second GDN state and its own KV cells for the same positions).

Lesson: a change that alters the expert mix changes the text, so compare ms per window (winms.py / summ.py) over many
prompts; and the exact protocol (no PCIe share) can show gains the installed config does not.

## Option 1, the measured go / no-go (a second branch for the bonus)

`card1_cost.py` on the pipeline trace (today's config, 8 prompts):

- card 1's window: 8.66 ms at 1 row, 9.76 at 2, 11.09 at 3 (fresh; speculative +0.1-0.2 ms) - **~1.1-1.3 ms per row**;
- after a wrong guess card 1 sits idle 11.8 ms (median; 10th percentile 9.9) before the corrected window: extra rows
  there are hidden;
- after a right guess card 1 is already the later card: it finishes B a median 1.08 ms after card 2 finishes A. One
  more row (+1.2 ms) makes card 2 wait +0.94 ms per right guess, two rows (+2.4 ms) +2.03 ms.

A branch in every window would tax every right guess. `alt_policy.py` on `STRATA_MTP_TOP2_LOG` (1,848 launched B
windows: bonus right 992, second choice right in 296 of the 856 misses) - the branch only when the drafter's probability
of its bonus p1 < x, a rescue worth ~11.5 ms (a kept speculative window instead of a fresh one), over ~35 s of decode:

| branch when | windows with it | rescued | right guesses taxed | net, +1 row | net, +2 rows |
|---|---|---|---|---|---|
| p1 < 0.5 | 808 | 158 | 252 | +4.5% | +3.7% |
| p1 < 0.7 | 1,306 | 266 | 510 | +7.4% | +5.8% |
| p1 < 0.9 | 1,456 | 287 | 616 | +7.8% | +5.9% |
| always | 1,848 | 296 | 992 | +7.1% | +4.0% |

An upper estimate (the GDN state copy for the branch and a 1-row branch's fewer tokens are not in it): **go**, at
~+4-6% expected.
