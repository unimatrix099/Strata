#!/usr/bin/env python3
# a rocprofv3 --kernel-trace of the decode (see README): python3 klayer.py <trace dir> [decode windows of the last request]
import csv, sys
P = sys.argv[1]
rows = []
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").replace("strata::kernels::", "").split("(")[0]
    rows.append((int(r["Start_Timestamp"]), int(r["End_Timestamp"]), r["Agent_Id"], n, r["Grid_Size_X"], r["Stream_Id"]))
rows.sort()
last_pf = max(x[1] for x in rows if "mul_mat_q" in x[3] or "prefill" in x[3])
dec = [x for x in rows if x[0] > last_pf]
for a in sorted({x[2] for x in dec}):
    ks = [x for x in dec if x[2] == a]
    r = [i for i, x in enumerate(ks) if x[3] == "route"]
    # print two consecutive layers from the middle
    m = len(r) // 2
    for i0, i1 in ((r[m], r[m+1]), (r[m+1], r[m+2])):
        seg = ks[i0:i1]
        print(f"== {a}: layer from route to route: {len(seg)} kernels, kernel time {sum(x[1]-x[0] for x in seg)/1e3:.1f} us")
        for j, x in enumerate(seg):
            print(f"{j:3d} {(x[1]-x[0])/1e3:6.1f}us gap-before {(x[0]-seg[j-1][1])/1e3 if j else 0:6.1f} s{x[5]} grid {x[4]:>6}  {x[3][:80]}")
    break
