#!/usr/bin/env python3
# a STRATA_PIPELINE_TRACE file -> the draft chain, the gaps and the cycle after an on-path verdict vs another: python3 pltrace_sum.py <trace>
import sys, statistics
reqs, cur = [], []
for line in open(sys.argv[1]):
    if line.startswith("#"):
        if cur: reqs.append(cur)
        cur = []; continue
    p = line.split()
    if len(p) >= 4: cur.append((float(p[0]), p[1], int(p[2]), int(p[3]), int(p[4]) if len(p) > 4 else 0))
if cur: reqs.append(cur)
chain, gap_v_cl, gap_cd_l0, cyc_on, cyc_off, s1_on = [], [], [], [], [], []
for r in reqs:
    last_cl = last_v = None
    for i, (t, c, seq, T, f) in enumerate(r):
        if c == "V": last_v = (t, f)
        if c == "CL":
            last_cl = t
            if last_v: gap_v_cl.append(t - last_v[0])
        if c == "CD" and last_cl is not None:
            chain.append(t - last_cl)
            nxt = next((x for x in r[i+1:] if x[1] == "L0"), None)
            if nxt: gap_cd_l0.append(nxt[0] - t)
    vs = [(t, f) for t, c, s, T, f in r if c == "V"]
    for (t0, f0), (t1, f1) in zip(vs, vs[1:]):
        (cyc_on if f0 == 1 else cyc_off).append(t1 - t0)
def m(x): return f"median {statistics.median(x):5.1f}  mean {statistics.mean(x):5.1f}  n {len(x)}" if x else "-"
print("draft chain, launch -> done:        ", m(chain))
print("verdict -> chain launch:             ", m(gap_v_cl))
print("chain done -> next stage-0 launch:  ", m(gap_cd_l0))
print("cycle after an on-path verdict (V=1):", m(cyc_on))
print("cycle after another verdict:         ", m(cyc_off))
