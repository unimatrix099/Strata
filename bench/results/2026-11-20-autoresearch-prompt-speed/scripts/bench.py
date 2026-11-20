#!/usr/bin/env python3
"""bench.py PORT LABEL [reps] : decode (8 prompts x reps, 384 tok, greedy, thinking on) + prefill 4K x2, 16K, 32K. Appends a row to results.tsv"""
import json, statistics as st, sys, time, urllib.request, os
port, label = sys.argv[1], sys.argv[2]; reps = int(sys.argv[3]) if len(sys.argv) > 3 else 2
mode = sys.argv[4] if len(sys.argv) > 4 else "all"
P = {"story": "Write a short story (about 400 words) about a lighthouse keeper who finds a message in a bottle.",
 "code": "Write a Python implementation of an LRU cache class with get and put methods, O(1) each, with docstrings and a short usage example.",
 "explain": "Explain how a refrigerator works to a 12-year-old, in about 300 words.",
 "email": "Write a polite email to a landlord asking to fix a broken heater, mentioning it has been a week.",
 "sql": "Write a SQL query that finds the top 5 customers by total order value in 2025, with table definitions.",
 "list": "Give me 15 ideas for a weekend trip near Munich, one line each with why it is worth it.",
 "math": "Solve step by step: a train leaves at 9:40 at 84 km/h, another at 10:10 at 105 km/h on the same track; when does the second catch up?",
 "rust": "Write a Rust function that parses a CSV line with quoted fields into a Vec<String>, with tests."}
def post(msg, n):
    b = {"messages": [{"role": "user", "content": msg}], "max_tokens": n, "temperature": 0, "seed": 1, "cache_prompt": False}
    r = urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions", json.dumps(b).encode(), {"Content-Type": "application/json"}), timeout=3600)
    return json.load(r)
post("Say hi.", 16)
dm = dmean = 0; pf = {}
if mode in ("all", "decode"):
    rows = {}
    for rep in range(reps):
        for k, p in P.items():
            d = post(p, 384); t = d["timings"]
            rows.setdefault(k, []).append(t["predicted_per_second"])
    per = {k: st.median(v) for k, v in rows.items()}
    print(label, "DECODE", {k: round(v, 1) for k, v in per.items()})
    dm, dmean = st.median(per.values()), st.mean(per.values())
    print(label, "DECODE median %.1f mean %.1f" % (dm, dmean), flush=True)
if mode in ("all", "prefill"):
    for n in ((4000, 4000, 4000, 16000, 16000, 32000, 32000) if os.environ.get('PF_LONG') else (4000, 4000, 16000, 32000)):
        txt = "".join(f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1, n // 17))
        d = post(f"[pf {time.time()}]\n" + txt + "Summarize in one sentence.", 8); t = d["timings"]
        pf.setdefault(n, []).append(t["prompt_per_second"])
        print(label, "PREFILL", n, "prompt_n", t["prompt_n"], "%.0f tok/s" % t["prompt_per_second"], flush=True)
row = [time.strftime("%H:%M"), label, "%.1f" % dm, "%.1f" % dmean] + ["%.0f" % st.median(pf.get(n, [0])) for n in (4000, 16000, 32000)]
if pf: print(label, "PREFILL median", {n: round(st.median(v)) for n, v in pf.items()}, flush=True)
new = not os.path.exists("/workspace/ar1120/results.tsv")
with open("/workspace/ar1120/results.tsv", "a") as f:
    if new: f.write("time\tlabel\tdec_median\tdec_mean\tpf4k\tpf16k\tpf32k\tstatus\tdescription\n")
    f.write("\t".join(row) + "\n")
