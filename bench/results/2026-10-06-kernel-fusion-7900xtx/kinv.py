#!/usr/bin/env python3
# a rocprofv3 --kernel-trace of the decode (see README): python3 kinv.py <trace dir> [decode windows of the last request]
import csv, sys, collections
P, W = sys.argv[1], int(sys.argv[2])
rows = []
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").replace("strata::kernels::", "").split("(")[0]
    rows.append((int(r["Start_Timestamp"]), int(r["End_Timestamp"]), r["Agent_Id"], n))
rows.sort()
last_pf = max(e for s, e, a, n in rows if "mul_mat_q" in n or "prefill" in n)
dec = [x for x in rows if x[0] > last_pf and "wait_flag" not in x[3]]
cnt = collections.Counter(); tim = collections.Counter()
for s, e, a, n in dec: cnt[n] += 1; tim[n] += (e - s) / 1e3
tot_k = sum(cnt.values()) / W; tot_t = sum(tim.values()) / W / 1e3
print(f"{tot_k:.0f} kernels, {tot_t:.2f} ms per window (both cards)")
print(f"{'per window':>10} {'avg us':>7} {'ms/win':>7}  kernel")
for n, c in sorted(cnt.items(), key=lambda x: -x[1])[:45]:
    print(f"{c/W:10.1f} {tim[n]/c:7.1f} {tim[n]/W/1e3:7.2f}  {n[:95]}")
