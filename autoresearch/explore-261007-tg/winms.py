#!/usr/bin/env python3
"""ms per decode window from bench.py engine logs: windows = generated - drafts accepted (each window emits its
accepted drafts plus one token). Usage: winms.py LABEL-PREFIX..."""
import glob, re, statistics, sys
LOGS = "bench/results/2026-10-07-upstream-port-7900xtx/data/logs/"
pat = re.compile(r"(\d+) generated in (\d+) ms .*drafts accepted (\d+) of (\d+)")
for pre in sys.argv[1:]:
    for f in sorted(glob.glob(LOGS + pre + "-*.engine.txt")):
        rows = [tuple(map(int, m.groups())) for m in map(pat.search, open(f, errors="replace")) if m and int(m.group(1)) >= 256]
        if not rows:
            continue
        ms = [t / (n - a) for n, t, a, d in rows]
        tpw = [n / (n - a) for n, t, a, d in rows]
        hit = [float(x) for x in re.findall(r"decode expert cache hit rate: ([\d.]+)%", open(f, errors="replace").read())[1:]]
        print(f"{f.split('/')[-1][:-11]:24} ms/window {statistics.median(ms):6.2f} (story {ms[0]:.2f}, code {ms[1]:.2f})  "
              f"tokens/window {statistics.median(tpw):.2f}  cache hit {statistics.median(hit):.1f}%")
