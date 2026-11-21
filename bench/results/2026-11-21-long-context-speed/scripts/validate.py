#!/usr/bin/env python3
"""validate.py PORT LABEL : needles at 3 depths in 8K/20K/40K/100K-token prompts, short known answers, decode speed after the long prompt"""
import json, sys, random, urllib.request
port, label = sys.argv[1], sys.argv[2]
def post(msg, n):
    b = {"messages": [{"role": "user", "content": msg}], "max_tokens": n, "temperature": 0, "seed": 1, "cache_prompt": False}
    r = urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions", json.dumps(b).encode(), {"Content-Type": "application/json"}), timeout=3600)
    return json.load(r)
ok = tot = 0
rnd = random.Random(1121)
for size in (30000, 128000, 236000):
    lines = [f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1, int(size / 19.16))]
    needles = {}
    for name, frac in (("Marlow", 0.1), ("Quillon", 0.5), ("Zephyrine", 0.9)):
        code = str(rnd.randint(100000, 999999)); needles[name] = code
        lines.insert(int(len(lines) * frac), f"NOTE: the secret code for {name} is {code}.\n")
    d = post(f"[v {size}]\n" + "".join(lines) + "\nWhat are the secret codes for Marlow, Quillon and Zephyrine? Answer with the three codes.", 1200)
    text = (d["choices"][0]["message"].get("content") or ""); t = d["timings"]
    hit = sum(c in text for c in needles.values()); ok += hit; tot += 3
    print(label, f"needles {size}: {hit}/3  prompt_n {t['prompt_n']} {t['prompt_per_second']:.0f} tok/s, decode {t['predicted_per_second']:.1f} tok/s ({t['predicted_n']} tok)", flush=True)
QA = [("What is 17 * 23? Answer with the number only.", "391"), ("What is the capital of Australia? One word.", "Canberra"),
      ("What is the chemical symbol for gold? Symbol only.", "Au"), ("How many days are in a leap year? Number only.", "366"),
      ("What is the square root of 1764? Number only.", "42"), ("Write the first 8 Fibonacci numbers starting 1, 1, comma separated.", "1, 1, 2, 3, 5, 8, 13, 21")]
for q, a in QA:
    d = post(q, 1500); text = d["choices"][0]["message"].get("content") or ""
    h = a in text; ok += h; tot += 1
    if not h: print(label, "MISS", q, "->", text[:120].replace("\n", " "))
print(label, f"VALIDATE {ok}/{tot}")
