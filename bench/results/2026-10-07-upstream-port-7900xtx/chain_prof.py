#!/usr/bin/env python3
# the MTP draft chain in a rocprofv3 --kernel-trace: python3 chain_prof.py <trace dir> "<Agent N>" <stream id> (the drafter's stream on the last card)
import csv, sys, collections, statistics
P, AG, SID = sys.argv[1], sys.argv[2], sys.argv[3]
rows = []
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").replace("strata::kernels::", "").split("(")[0]
    rows.append((int(r["Start_Timestamp"]), int(r["End_Timestamp"]), r["Agent_Id"], r["Stream_Id"], n, r["Grid_Size_X"]))
rows.sort()
last_pf = max(x[1] for x in rows if "mul_mat_q" in x[4] or "prefill" in x[4])
ks = [x for x in rows if x[0] > last_pf and x[2] == AG and x[3] == SID]
chains, cur = [], [ks[0]]
for p, q in zip(ks, ks[1:]):
    if q[0] - p[1] > 300_000: chains.append(cur); cur = [q]      # a gap over 300 us starts a new chain
    else: cur.append(q)
chains.append(cur)
cnt = [len(c) for c in chains]; span = [(c[-1][1] - c[0][0]) / 1e6 for c in chains]
busy = [sum(e - s for s, e, *_ in c) / 1e6 for c in chains]
print(f"{len(chains)} chains; kernels per chain median {statistics.median(cnt)}, span median {statistics.median(span):.2f} ms "
      f"(mean {statistics.mean(span):.2f}), kernel time median {statistics.median(busy):.2f} ms")
agg = collections.Counter(); num = collections.Counter()
for c in chains:
    for s, e, a, sid, n, g in c: agg[n[:70]] += (e - s) / 1e3; num[n[:70]] += 1
N = len(chains)
print(f"{'per chain':>9} {'avg us':>7} {'us/chain':>9}  kernel")
for n, t in agg.most_common(22):
    print(f"{num[n]/N:9.1f} {t/num[n]:7.1f} {t/N:9.1f}  {n}")
mid = chains[len(chains)//2]
print(f"\none chain in order ({len(mid)} kernels):")
for j, (s, e, a, sid, n, g) in enumerate(mid[:80]):
    gap = (s - mid[j-1][1]) / 1e3 if j else 0
    print(f"{j:3d} {(e-s)/1e3:6.1f}us gap {gap:5.1f} grid {g:>7}  {n[:70]}")
