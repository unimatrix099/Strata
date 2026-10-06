#!/usr/bin/env python3
"""Card A (main) and card B (peer) in a rocprofv3 --kernel-trace of the peer tier's decode: each card's kernel
time per window (the last request after its prompt) and card B's per-layer bursts.
python3 peer_bound.py <trace dir> <decode windows of that request, from STRATA_DECODE_TIMING>"""
import csv, sys, collections, statistics
P, W = sys.argv[1], int(sys.argv[2])
rows = []
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").split("(")[0]
    rows.append((int(r["Start_Timestamp"]), int(r["End_Timestamp"]), r["Agent_Id"], n))
rows.sort()
last_pf = max(e for s, e, a, n in rows if "mul_mat_q" in n or "prefill" in n)
dec = [x for x in rows if x[0] > last_pf]
agents = sorted({a for _, _, a, _ in dec}, key=lambda a: -sum(1 for x in dec if x[2] == a))
A, Bp = agents[0], agents[1]                      # A: the main card (most kernels), B: the peer
for name, ag in (("card A (main)", A), ("card B (peer)", Bp)):
    ks = [x for x in dec if x[2] == ag]
    work = [x for x in ks if "wait_flag" not in x[3]]
    wait = sum(e - s for s, e, a, n in ks if "wait_flag" in n) / W / 1e6
    print(f"{name}: {len(work)/W:.0f} work kernels per window, kernel time {sum(e-s for s,e,a,n in work)/W/1e6:.2f} ms, "
          f"in wait_flag {wait:.2f} ms")
    top = collections.Counter()
    for s, e, a, n in work: top[n[:60]] += (e - s) / W / 1e6
    print("   ", "; ".join(f"{n} {t:.2f}" for n, t in top.most_common(5)))
# card B's per-layer bursts: kernels closer than 30 us belong to one layer's share
b = [x for x in dec if x[2] == Bp and "wait_flag" not in x[3]]
bursts, cur = [], None
for s, e, a, n in b:
    if cur and s - cur[1] < 30000:
        cur = (cur[0], max(cur[1], e), cur[2] + (e - s))
    else:
        if cur: bursts.append(cur)
        cur = (s, e, e - s)
if cur: bursts.append(cur)
span = [(e - s) / 1e3 for s, e, k in bursts]
busy = [k / 1e3 for s, e, k in bursts]
print(f"card B: {len(bursts)/W:.1f} bursts per window (one per layer it serves); per burst: kernel time median "
      f"{statistics.median(busy):.1f} us (p90 {sorted(busy)[int(.9*len(busy))]:.1f}), first-to-last span median "
      f"{statistics.median(span):.1f} us")
print(f"card B: expert work per window {sum(busy)/W/1e3:.2f} ms")
