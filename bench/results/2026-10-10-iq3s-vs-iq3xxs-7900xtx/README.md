# IQ3_S against IQ3_XXS on 2x RX 7900 XTX (10 Oct)

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
