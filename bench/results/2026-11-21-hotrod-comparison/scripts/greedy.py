#!/usr/bin/env python3
"""Same-protocol greedy bench for both engines: greedy.py PORT LABEL   (8 prompts x3 reps, 512 tok, default template (thinking on, as bench.py); + 32K prefill)
Prompts = bench_many.py's eight. Rates are the engine's own timings (predicted_per_second / prompt_per_second)."""
import json, statistics as st, sys, urllib.request
port, label = sys.argv[1], sys.argv[2]
P = {"story": "Write a short story (about 400 words) about a lighthouse keeper who finds a message in a bottle.",
 "code": "Write a Python implementation of an LRU cache class with get and put methods, O(1) each, with docstrings and a short usage example.",
 "explain": "Explain how a refrigerator works to a 12-year-old, in about 300 words.",
 "email": "Write a polite email to a landlord asking to fix a broken heater, mentioning it has been a week.",
 "sql": "Write a SQL query that finds the top 5 customers by total order value in 2025, with table definitions.",
 "list": "Give me 15 ideas for a weekend trip near Munich, one line each with why it is worth it.",
 "math": "Solve step by step: a train leaves at 9:40 at 84 km/h, another at 10:10 at 105 km/h on the same track; when does the second catch up?",
 "rust": "Write a Rust function that parses a CSV line with quoted fields into a Vec<String>, with tests."}
def post(msg, n, tag):
    b = {"messages": [{"role": "user", "content": msg}], "max_tokens": n, "temperature": 0, "seed": 1,
         "cache_prompt": False}
    r = urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions", json.dumps(b).encode(), {"Content-Type": "application/json"}), timeout=3600)
    return json.load(r)
post("Say hi.", 16, "warm")
rows = {}
for rep in range(3):
    for k, p in P.items():
        d = post(p, 512, k); t = d["timings"]
        rows.setdefault(k, []).append((t["predicted_per_second"], d["usage"]["completion_tokens"]))
        print(label, rep, k, round(t["predicted_per_second"], 1), d["usage"]["completion_tokens"], flush=True)
per = {k: st.median(x[0] for x in v) for k, v in rows.items()}
print(label, "DECODE per-prompt median:", {k: round(v, 1) for k, v in per.items()})
print(label, "DECODE median of 8 prompts: %.1f   mean: %.1f" % (st.median(per.values()), st.mean(per.values())))
rec = "".join(f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1, 2000))
for n in (4000, 32000):
    txt = "".join(f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1, n // 17))
    d = post(f"[prefill {n}] " + txt + "Summarize in one sentence.", 32, "pf"); t = d["timings"]
    print(label, f"PREFILL ~{n}: prompt_n={t['prompt_n']} {t['prompt_per_second']:.0f} tok/s (cache_n={t.get('cache_n')})  decode {t['predicted_per_second']:.1f}", flush=True)
