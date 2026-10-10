# IQ3_S and Swift 1.5 IQ3_S against IQ3_XXS on 2x RX 7900 XTX (10 Oct)

Both GSQ-RCO files of Qwen3.8-Flash-Next, the same engine and the same tuned config (`strata-iq3_s-tuned.json` is the
production config with IQ3_S's files; setup's own IQ3_S config has none of the tuning). Ryzen 9 5950X, 128 GB,
ROCm 7.9. How it ran: `m1.sh`; the quality check: `qeval.py`.

**Decode** (eight prompts, 512 tokens, greedy, three alternating rounds): IQ3_XXS 117.1 / 119.7 / 116.7 tok/s, IQ3_S
109.9 / 113.2 / 115.6 - ratios 0.941, 0.946, 0.993: **IQ3_S ~4% slower** (5 of 24 prompts faster). Its experts
(50 GB against 43) leave 17,960 expert slots in VRAM against 21,560.

**Prompt reading** (two rounds each):

| prompt | IQ3_XXS | IQ3_S | IQ3_S |
|---|---|---|---|
| 1K | 652 / 708 tok/s | 692 / 692 | about the same |
| 4K | 1,464 / 1,515 | 1,144 / 1,095 | -25% |
| 16K | 2,419 / 2,455 | 2,012 / 2,069 | -16% |
| 32K | 3,044 / 3,107 (first token 10 s) | 2,575 / 2,590 (12 s) | -16% |

**Quality** - a known-answer check, greedy, thinking on, 12,000 tokens at most per answer:

| set | IQ3_XXS | IQ3_S |
|---|---|---|
| easy: 31 math word problems + 8 Python tasks with unit tests | 31/31, 8/8 | 30/31 (one arithmetic slip: 314 for 316), 8/8 |
| hard: 32 exact multi-step computations (modular powers, sums of primes, divisor counts, derangements, lattice paths, dice probabilities, digit sums, Fibonacci mod m) + 8 harder Python tasks | 29/32, 7/8 | 30/32, 8/8 |
| all 79 | 75 | 76 |
| tokens used on the hard set | 119,926 | 94,407 (-21%) |

Every miss on the hard set, for both, is an answer that ran out of the 12,000-token budget while still thinking (the
same two math items for both; IQ3_XXS also on one more math item and one code task) - none was a wrong final answer.
IQ3_S reached its answers with ~21% fewer thinking tokens. The difference in score (one item) is inside what one
run of 79 items can tell apart.

**In short**: on this PC IQ3_S decodes ~4% slower and reads prompts above ~2K tokens 16-25% slower; on these 79 items
it is not measurably more accurate, but it thinks shorter on hard problems (-21% tokens), which takes back part of its
slower decode on such questions. IQ3_XXS stays the production model; IQ3_S is installed (`run-iq3_s.sh` with setup's
defaults, `strata-iq3_s-tuned.json` with the production tuning) and needs only its first 54.8 GB shard (the second is
shared with IQ3_XXS).

## Swift 1.5 IQ3_S (UkisAI's fine-tune that thinks shorter), installed by hand

Setup does not offer Swift 1.5 in IQ3_S (its list predates the file), so it was installed by hand: both shards from
`ukisai/Swift-1.5-Qwen3.8-Flash-Next-GSQ-RCO-GGUF` (44.9 + 38.8 GB, SHA-256 checked), the pack with setup's
`tools/iq_pack.py` (one layer's experts span both shards: native_experts v4), and `strata-swift-iq3_s-tuned.json` (the
production config with Swift's files). Swift's files are laid out the other way round from ISTA's: the per-layer
embedding table is in shard **1** and most experts in shard 2, so `--ple-gguf` points at shard 1. The MTP draft layer
and the expert profile are the shared ones, as setup does for Swift. How it ran: `m2.sh`.

| | IQ3_XXS | IQ3_S | Swift 1.5 IQ3_S |
|---|---|---|---|
| decode, eight prompts, three rounds against IQ3_XXS | - | 0.941 / 0.946 / 0.993 (-4%) | 0.987 / 0.936 / 0.843 (-8%, noisy) |
| 1K prompt | 652-708 tok/s | 692 | 667-678 |
| 4K | 1,464-1,515 | 1,095-1,144 | 1,073-1,130 |
| 16K | 2,419-2,455 | 2,012-2,069 | 2,045-2,055 |
| 32K | 3,044-3,107 | 2,575-2,590 | 2,517-2,598 |
| easy set (39) | 39 | 38 | **39** |
| hard set (40) | 36 | 38 | **39** |
| all 79 | 75 | 76 | **78** |
| thinking + answer tokens, hard set | 119,926 | 94,407 (-21%) | **76,099 (-37%)** |
| answers that ran out of the 12,000-token budget | 4 | 2 | **1** |
| wall time for the hard set | 839 s | 700 s | **570 s (-32%)** |
| wall time for the easy set | 121 s | 113 s | 111 s |

Swift 1.5 decodes and reads prompts at IQ3_S's speed (the same 50 GB of experts), but it thinks ~37% shorter on
the hard problems than IQ3_XXS, so it finishes them ~32% sooner and solves more of them inside the budget (78 of 79,
the only miss a budget-out on an item every model ran out on). On short questions the thinking is short for all
three, and IQ3_XXS's faster decode and prompt reading weigh more. The authors' claim (-63% thinking tokens) was at
their highest effort level; here, with default settings, -37% on the hard set and -16% on the easy one (11,455 against
13,578 tokens). Licence: the Swift Open License 1.0 plus the Qwen Community License for the original's parts.
