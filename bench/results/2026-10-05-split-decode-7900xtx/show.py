#!/usr/bin/env python3
"""Print the bench results: label, median story / code / all tok/s, and each run's hash (prompt:rep:sha)."""
import json, os, sys
path = os.path.join(os.path.dirname(__file__), "data", "results.jsonl")
want = sys.argv[1:]
for line in open(path):
    r = json.loads(line)
    if want and not any(r["label"].startswith(w) for w in want):
        continue
    if "error" in r:
        print(f'{r["label"]:28s} ERROR {r["error"]}'); continue
    runs = " ".join(f'{x["prompt"][0]}{x["rep"]}:{x["sha"][:6]}:{x["tok_s"]}' for x in r["runs"])
    if "prefill" in r:
        print(f'{r["label"]:28s} prefill {r["prefill"]:7.1f} tok/s | {runs}'); continue
    print(f'{r["label"]:28s} story {r["story"]:5.1f} code {r["code"]:5.1f} all {r["median"]:5.1f} | {runs}')
