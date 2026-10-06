#!/usr/bin/env python3
# windows-weighted ms/window, verify and GPU-reach wait from STRATA_DECODE_TIMING lines: python3 winms.py <logs dir> <label>...
import re, sys, glob, os
# per label: the decode timing lines of its latest log -> windows-weighted ms/window and GPU-reach wait
for lab in sys.argv[2:]:
    f = sorted(glob.glob(f"{sys.argv[1]}/{lab}-*.engine.txt"), key=os.path.getmtime)[-1]
    rows = [tuple(map(float, m)) for m in re.findall(r"decode timing: (\d+) windows, avg T [\d.]+, [\d.]+ tokens/window, ([\d.]+) ms/window = verify ([\d.]+) \(GPU-reach wait ([\d.]+)", open(f).read())]
    w = sum(r[0] for r in rows)
    print(f"{lab:14s} {int(w)} windows: {sum(r[0]*r[1] for r in rows)/w:.2f} ms/window, verify {sum(r[0]*r[2] for r in rows)/w:.2f}, GPU-reach wait {sum(r[0]*r[3] for r in rows)/w:.2f}")
