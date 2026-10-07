#!/usr/bin/env python3
"""Simulate 'a second branch (the drafter's second choice) only when the bonus guess's probability p1 < x' from a
STRATA_MTP_TOP2_LOG file. gain: a rescued window (bonus wrong, second choice right) saves SAVE ms; cost: every right
guess that also ran the branch makes card 2 wait WAIT ms more (card1_cost.py: +0.94 ms for one extra row, +2.03 for
two). Only windows whose B was launched (p_on >= theta) count. Usage: alt_policy.py LOG TOTAL_MS"""
import sys
SAVE, THETA = 11.5, 0.1
rows = [tuple(map(float, l.split())) for l in open(sys.argv[1])]
total = float(sys.argv[2])
rows = [r for r in rows if r[1] >= THETA]
print(f"{len(rows)} launched B windows; bonus right {sum(r[2] for r in rows):.0f}, second choice right {sum(r[3] for r in rows):.0f}")
for wait, name in ((0.94, "+1 row"), (2.03, "+2 rows")):
    print(name)
    for x in (0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.01):
        sel = [r for r in rows if r[0] < x]
        rescued = sum(1 for r in sel if r[2] == 0 and r[3] == 1)
        cost_n = sum(1 for r in sel if r[2] == 1)
        net = rescued * SAVE - cost_n * wait
        print(f"  p1 < {x:4.2f}: branch in {len(sel):4d} windows, rescues {rescued:3d}, taxes {cost_n:4d} right guesses: "
              f"net {net / 1000:+.2f} s = {100 * net / total:+.1f}%")
