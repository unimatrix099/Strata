#!/usr/bin/env python3
"""From a STRATA_PIPELINE_TRACE: card 1's window time by row count, and on each wrong guess how long card 1 sat idle
between the discarded speculative window's end (F0) and the corrected window's launch (L0)."""
import collections, statistics, sys
dur = collections.defaultdict(list); idle = []; busy_wait = 0
for req in open(sys.argv[1]).read().split("# request")[1:]:
    ev = [l.split() for l in req.strip().splitlines()[1:]]
    l0 = {}; f0 = {}; spec = {}; undone = []
    for e in ev:
        t, k, seq = float(e[0]), e[1], int(e[2])
        if k == "L0": l0[seq] = (t, int(e[3])); spec[seq] = e[4] == "1"
        elif k == "F0" and seq in l0: f0[seq] = t; dur[(l0[seq][1], spec[seq])].append(t - l0[seq][0])
        elif k == "U": undone.append((t, seq))
    # after an undo of window s (rolled back), the corrected window is the next L0 after the undo
    for tu, s in undone:
        nxt = [(t, q) for q, (t, T) in l0.items() if t >= tu]
        if nxt and s in f0:
            t, q = min(nxt)
            idle.append(t - f0[s])
for (T, sp), v in sorted(dur.items()):
    if len(v) >= 10:
        print(f"card 1 window, {T} rows, {'speculative' if sp else 'fresh'}: {statistics.median(v):6.2f} ms median ({len(v)})")
if idle:
    q = statistics.quantiles(idle, n=10)
    print(f"wrong guesses: {len(idle)}; card 1 idle between the discarded window's end and the corrected launch: "
          f"median {statistics.median(idle):.2f} ms, 10th pct {q[0]:.2f}, 90th {q[-1]:.2f}")

# on-path windows: card 1 finished B (F0 B) vs card 2 finishing A (F1 A) - positive slack: card 1 was done first
slack = []
for req in open(sys.argv[1]).read().split("# request")[1:]:
    ev = [l.split() for l in req.strip().splitlines()[1:]]
    f0 = {}; f1 = {}; on = []
    for e in ev:
        t, k, seq = float(e[0]), e[1], int(e[2])
        if k == "F0": f0[seq] = t
        elif k == "F1": f1[seq] = t
        elif k == "V" and e[4] == "1": on.append(seq)
    for a in on:
        if a in f1 and a + 1 in f0:
            slack.append(f1[a] - f0[a + 1])
if slack:
    q = statistics.quantiles(slack, n=10)
    print(f"right guesses: {len(slack)}; card 2 finishes A this long after card 1 finishes B: median "
          f"{statistics.median(slack):.2f} ms, 10th pct {q[0]:.2f}, 25th {statistics.quantiles(slack, n=4)[0]:.2f}, 90th {q[-1]:.2f}")
    for extra in (1.2, 2.4, 3.6):
        lost = sum(max(0.0, extra - max(0.0, s)) for s in slack) / len(slack)
        print(f"  +{extra} ms on card 1's B: card 2 would wait {lost:.2f} ms more per right guess on average")
