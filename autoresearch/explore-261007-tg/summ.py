#!/usr/bin/env python3
"""Summarize bench_many.py runs: total tok/s, ms per window, tokens per window, cache hit range. summ.py PREFIX..."""
import json, re, sys
for l in open('autoresearch/explore-261007-tg/results.jsonl'):
    r = json.loads(l)
    if not any(r['label'].startswith(p) for p in sys.argv[1:]): continue
    runs = r.get('runs', [])
    if not runs: print(r['label'], r.get('error')); continue
    tok = sum(x['n'] for x in runs); sec = sum(x['n'] / x['tok_s'] for x in runs)
    txt = open(r['log'], errors='replace').read()
    rows = [tuple(map(int, m)) for m in re.findall(r"(\d+) generated in (\d+) ms .*?drafts accepted (\d+) of", txt) if int(m[0]) >= 64]
    n = sum(a for a, b, c in rows); t = sum(b for a, b, c in rows); acc = sum(c for a, b, c in rows)
    hit = [float(x) for x in re.findall(r"decode expert cache hit rate: ([\d.]+)%", txt)[1:]]
    print(f"{r['label']:16} total {tok/sec:6.1f} tok/s  ms/window {t/(n-acc):5.2f}  tokens/window {n/(n-acc):.2f}  "
          f"hit {min(hit):.1f}-{max(hit):.1f}%  {r.get('error','')}")
