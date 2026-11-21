#!/usr/bin/env python3
"""summ.py LABEL : prompt phases of the 12K-token chunks at the deepest positions (per card) from the engine timing lines"""
import re, sys
keys=["qsa select","qsa attn","kv stage","wait copy","gemm gate/up"]
rows=[]
for l in open(f"/workspace/ar1121/run/{sys.argv[1]}-engine.txt", errors="replace"):
    m=re.search(r"prefill timing: (\d+) tokens, GPU timeline (\d+) ms, wall (\d+) ms, host staging (\d+) ms:(.*)",l)
    if not m: continue
    ph=dict((a.strip(),int(b)) for a,b in re.findall(r"([a-z+/: ]+?) (\d+) \(",m.group(5)))
    rows.append((int(m.group(1)),int(m.group(2)),ph))
big=[r for r in rows if r[0]>100000]
for r in big: print(sys.argv[1],"TOTAL",r[0],"gpu",r[1]," ".join(f"{k}={r[2].get(k,0)}" for k in keys))
ch=[r for r in rows if r[0]==12032]
if ch:
    r=max(ch,key=lambda r:r[1]); print(sys.argv[1],"DEEPEST 12K chunk gpu",r[1]," ".join(f"{k}={r[2].get(k,0)}" for k in keys))
