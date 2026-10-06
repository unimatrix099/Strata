#!/usr/bin/env python3
# a rocprofv3 --kernel-trace of the decode (see README): python3 kpairs.py <trace dir> [decode windows of the last request]
import csv, sys, collections
P, W = sys.argv[1], int(sys.argv[2])
rows = []
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").replace("strata::kernels::", "").split("(")[0]
    rows.append((int(r["Start_Timestamp"]), int(r["End_Timestamp"]), r["Agent_Id"], n, r["Stream_Id"]))
rows.sort()
last_pf = max(x[1] for x in rows if "mul_mat_q" in x[3] or "prefill" in x[3])
dec = [x for x in rows if x[0] > last_pf]
pairs = collections.Counter()
for a in {x[2] for x in dec}:
    ks = [x for x in dec if x[2] == a]
    for p, q in zip(ks, ks[1:]):
        if p[4] == q[4]: pairs[(p[3][:55], q[3][:55])] += 1
print("most frequent back-to-back pairs (same stream), per window:")
for (p, q), c in pairs.most_common(28):
    print(f"{c/W:6.1f}  {p}  ->  {q}")
