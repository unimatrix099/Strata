#!/usr/bin/env python3
"""Bound for a tensor split from a rocprofv3 --kernel-trace of the layer split's decode: every kernel of the last request
after its prompt, per card and window, and the kernel time if each card ran all of them at half the work (down to
a floor).  python3 tp_bound.py <trace dir> <decode windows of that request, from STRATA_DECODE_TIMING>"""
import csv, sys, collections
P, W = sys.argv[1], int(sys.argv[2])          # trace dir, decode windows of the last request (engine log)
rows = []
for r in csv.DictReader(open(P + "/run_kernel_trace.csv")):
    n = r["Kernel_Name"].replace("(anonymous namespace)::", "").split("(")[0]
    rows.append((int(r["Start_Timestamp"]), int(r["End_Timestamp"]), r["Agent_Id"], n))
rows.sort()
last_pf = max(e for s, e, a, n in rows if "mul_mat_q" in n or "prefill" in n)
dec = [x for x in rows if x[0] > last_pf and "wait_flag" not in x[3]]
per = collections.defaultdict(list)
for s, e, a, n in dec: per[a].append((e - s) / 1e3)   # us
print(f"decode windows: {W}")
tot_now = 0; kern = 0; small = 0
for a, d in sorted(per.items()):
    k = len(d) / W; t = sum(d) / W / 1e3; sm = sum(x for x in d if x < 8) / W / 1e3
    tot_now += t; kern += k; small += sm
    big = sorted(d, reverse=True)
    print(f"{a}: {k:.0f} kernels/window, kernel time {t:.2f} ms/window; in kernels < 8 us: {sm:.2f} ms "
          f"({100*sum(1 for x in d if x < 8)/len(d):.0f}% of kernels); median {big[len(big)//2]:.1f} us")
print(f"both cards in turn: {kern:.0f} kernels, {tot_now:.2f} ms of kernel time per window")
for floor in (4.0, 8.0):
    tp = sum(max(x / 2, min(x, floor)) for v in per.values() for x in v) / W / 1e3
    print(f"tensor split, every kernel halves down to a floor of {floor:.0f} us: {tp:.2f} ms of kernel time per card per window "
          f"(each card runs all {kern:.0f} kernels)")
