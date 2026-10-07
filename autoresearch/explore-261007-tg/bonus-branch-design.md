# A second branch for the bonus token - design notes (07 Oct 2026, branch `bonus-branch`)

The pipeline (`--pipeline-windows 2`) runs window B on card 1 on the guess that window A is accepted whole and that the
target's bonus token is the drafter's first choice. 37.9% of windows are accepted whole with the bonus wrong; the
drafter's second choice is the bonus in 31% of those (`STRATA_MTP_TOP2=1`). The idea: run B' (the second choice and its
drafts) beside B, and keep whichever the verdict confirms.

## What a window writes (src/core/verify.cpp; mapped by reading the code)

| state | written | conflict for B and B' at the same positions |
|---|---|---|
| GDN state | only at commit (the window reads it) | none |
| attention K/V cells (+ the streaming host copy at identity addresses) | in the window, read back by the window's own rows | yes: one cell per position |
| attention indexer tail / pooled blocks | in the window | yes |
| PLE history | in the window (row by row) | yes |
| recorded commit inputs, tail / history snapshots, hand-off rows | per Verifier object | needs a third verifier, or rows in one |
| the drafter's K/V | each chain rewrites the cells it reads | none (no rollback needed) |

## Two ways to run B'

**B' as its own window after B on card 1** (a third stage-0 verifier and hand-off; restore the indexer tail and PLE
history from B's snapshots before B'; the winner's commit re-appends its K/V). Card 1 then spends a whole window
(~8.7 ms at 1 row) on B'. On a right guess card 1 is idle only ~2-3 ms before the next window needs it, so B' delays
it by ~6 ms: 510 taxed right guesses x ~6 ms = ~3.1 s against ~2.7 s rescued (p1 < 0.7, 1,848 windows, ~35 s).
**Net zero or negative.**

**B' as extra rows in B's window** (~1.1-1.3 ms per row, the +5.8-7.4% simulation): the rows of the two branches must
not see each other - the GDN recurrence grouped per branch (the batch-slot path does this per slot), the attention rows
of B' on their own cells (a shadow page per QSA layer for the page holding p, pre-filled with the cells before p, or a
row-exclusion mask in the attention and selection kernels), the indexer tail and pooled blocks per branch, the PLE
history per branch, the commit replaying the winner's rows, the hand-off giving stage 1 the winner's rows, the KV
streaming host copy rewritten by the winner. Upstream keeps the batch-slot machinery out of the pipelined windows by
design.

## The drafter's side (src/core/mtp.cpp)

A branch chain can resume from step `base` instead of re-running the prefix: save `Rin_[0]` after step `base`, restore
it with `tok_[0]` = the second choice, launch steps `base+1..base+m` (~0.5 ms each) into their own output slots and
event. Or a `force_token` sentinel that reads the second choice on the device, so both branches enqueue back to back.

## Milestones (B' as extra rows)

1. Branch rows in one pipelined stage-0 window with the second branch inert: B's outputs bitwise unchanged with B'
   rows present (exactness test), card 1's time per extra row measured.
2. B' rows correct: their hand-off rows equal a serial window over the same tokens (bitwise).
3. Commit / hand-off of the winner; the pipeline control (launch B' when p1 < 0.7, promote it at the verdict).
4. The drafter's branch chain; the speed measured over 8 prompts, text identical to the serial loop.
