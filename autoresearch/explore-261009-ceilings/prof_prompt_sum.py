#!/usr/bin/env python3
"""Top kernels of a rocprofv3 kernel trace by total time (both agents), with counts and average duration."""
import csv, collections, sys
P = sys.argv[1]
agg = collections.Counter(); num = collections.Counter(); per = collections.Counter()
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").replace("strata::kernels::", "").replace("strata::prefill::", "").split("(")[0][:70]
    d = int(r["End_Timestamp"]) - int(r["Start_Timestamp"])
    agg[(r["Agent_Id"], n)] += d; num[(r["Agent_Id"], n)] += 1
tot = sum(agg.values())
print(f"total kernel time {tot/1e9:.2f} s")
for (a, n), t in agg.most_common(28):
    print(f"{t/1e9:7.3f} s {100*t/tot:5.1f}% {num[(a,n)]:7d} x {t/num[(a,n)]/1e3:8.1f} us  {a}  {n}")
