#!/usr/bin/env python3
"""ctx.py PORT LABEL [sizes] : prompt speed and decode speed against context size.
Per size: one cold long prompt (prompt tok/s + decode), then 2 more answers on the cached prefix (decode only).
Appends a row to results.tsv."""
import hashlib, re, json, statistics as st, sys, time, urllib.request, os
port, label = sys.argv[1], sys.argv[2]
sizes = [int(x) for x in (sys.argv[3] if len(sys.argv) > 3 else "8000,128000,240000").split(",")]
NTOK = int(os.environ.get("CTX_NTOK", "300"))
TAILS = ["Ignore the records above. Write a short story (about 400 words) about a lighthouse keeper who finds a message in a bottle.",
         "Ignore the records above. Write a Python implementation of an LRU cache class with get and put methods, O(1) each, with docstrings.",
         "Ignore the records above. Explain how a refrigerator works to a 12-year-old, in about 300 words."]
def post(msg, n, cache):
    b = {"messages": [{"role": "user", "content": msg}], "max_tokens": n, "temperature": 0, "seed": 1, "cache_prompt": cache}
    r = urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions", json.dumps(b).encode(), {"Content-Type": "application/json"}), timeout=3600)
    return json.load(r)
post("Say hi.", 16, False)
row = [time.strftime("%H:%M"), label]
for n in sizes:
    stamp = f"[ctx {n}]\n"
    txt = stamp + "".join(f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1, int(n / 19.16)))
    dec = []; msw = []; pps = 0; pn = 0
    for j, tail in enumerate(TAILS):
        t0 = time.time(); d = post(txt + tail, NTOK, True); t = d["timings"]
        if j == 0: pps, pn = t["prompt_per_second"], t["prompt_n"]
        dec.append(t["predicted_per_second"])
        w = [l for l in open(f"/workspace/ar1121/run/{label}-engine.txt", errors="replace") if "decode timing" in l]
        mw = float(re.search(r"([0-9.]+) ms/window", w[-1]).group(1)) if w else 0; tw = float(re.search(r"([0-9.]+) tokens/window", w[-1]).group(1)) if w else 0
        msw.append(mw)
        print(label, "CTXRUN", n, "tail", j, "prompt_n", t["prompt_n"], "%.0f tok/s" % t["prompt_per_second"], "decode %.1f" % t["predicted_per_second"], "n", t["predicted_n"], "wall %.1f" % (time.time() - t0), "ms/window %.2f tok/window %.2f" % (mw, tw), "sha", hashlib.sha1(json.dumps(d["choices"][0]["message"]).encode()).hexdigest()[:8], flush=True)
    print(label, "CTX", n, "prompt_n", pn, "prompt %.0f tok/s" % pps, "decode median %.1f" % st.median(dec), "ms/window mean %.2f" % st.mean(msw), msw, flush=True)
    row += ["%d" % pn, "%.0f" % pps, "%.1f" % st.median(dec), "%.2f" % st.mean(msw)]
new = not os.path.exists("/workspace/ar1121/results.tsv")
with open("/workspace/ar1121/results.tsv", "a") as f:
    if new: f.write("time\tlabel\t(prompt_n\tprompt_tps\tdecode_tps\tms_per_window) per size\n")
    f.write("\t".join(row) + "\n")
