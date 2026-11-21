#!/usr/bin/env python3
"""prof.py LABEL : decode profile lines -> QSA scores+topk / kv-resolve / attention and total, per request and stage"""
import re, sys
for l in open(f"/workspace/ar1121/run/{sys.argv[1]}-engine.txt", errors="replace"):
    if "decode GPU stages" not in l: continue
    q = l.split("QSA layers:")[1]
    g = lambda k: float(re.search(re.escape(k) + r" ([0-9.]+)", q).group(1))
    w = int(re.search(r"over (\d+) windows", l).group(1))
    if w < 100: continue
    print(sys.argv[1], "PROF", "stage1" if "stage 1" in l else "stage0", "scores+topk %.2f kv-resolve %.2f attention %.2f total %s" % (g("scores+topk"), g("kv-resolve"), g("attention"), re.search(r"total ([0-9.]+)", l).group(1)))
