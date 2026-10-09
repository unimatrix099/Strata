#!/usr/bin/env python3
"""Long-context quality and speed suite for the installed Strata config (2x RX 7900 XTX, IQ3_XXS, 128K context).

Starts serve/server.py with the installed config (or --exe / --arg / --drop-arg / --env variants), then runs:

  speed     prompt reading and decode speed, time to first token, at 4K..~120K (a unique prompt each, no cache reuse)
  needles   tools/needle_bench.py: a code word at depth 10/50/90% of 8K / 32K / 64K / ~128K
  facts     three facts spread through 32K / 64K / ~120K, a question that combines two (thinking on)
  longans   a ~4K-token report after 32K / 64K / ~120K of prompt (streamed: speed over the answer, degeneration checks)
  longgen   16K tokens from a short prompt: a story and a program, greedy and sampled
  turns     a conversation grown to ~100K over four parts (the prompt cache's path), then a recall question

Every answer is checked for degeneration: repetition loops, highly compressible stretches (a loop), repeated 8-word
n-grams, broken or unexpected-script characters. Results: <out>/results.jsonl, <out>/report.md, the answers in
<out>/answers/, the engine and server logs in <out>/logs/.

  longctx.py --label prod --out ~/strata-tools/longctx/prod
  longctx.py --label serial --drop-arg=--pipeline-windows --tests needles,facts
"""
import argparse, collections, json, os, random, re, signal, statistics, subprocess, sys, threading, time, urllib.request, zlib

ROOT = "/workspace"
sys.path.insert(0, os.path.join(ROOT, "tools"))
from needle_bench import haystack, CHARS_PER_TOKEN  # noqa: E402

PORT = 8093


# ------------------------------------------------------------------ the server
def start_server(a, out):
    cfg = json.load(open(os.path.join(ROOT, "strata-iq3_xxs.json")))
    args = list(cfg["args"])
    for flag in a.drop_arg:
        if flag in args:
            i = args.index(flag)
            has_val = i + 1 < len(args) and not args[i + 1].startswith("--")
            del args[i:i + (2 if has_val else 1)]
    cfg["args"] = args + a.arg
    if a.exe:
        cfg["exe"] = a.exe
    for kv in a.set:   # a config key, e.g. --set parallel=4 (the value as JSON)
        k, v = kv.split("=", 1)
        cfg[k] = json.loads(v)
    cfg["env"] = {**cfg.get("env", {}), **dict(e.split("=", 1) for e in a.env)}
    logs = os.path.join(out, "logs")
    os.makedirs(logs, exist_ok=True)
    cfg["log"] = os.path.join(logs, "engine.txt")
    cfg_path = os.path.join(logs, "config.json")
    json.dump(cfg, open(cfg_path, "w"), indent=1)
    srv_log = open(os.path.join(logs, "server.txt"), "w")
    py = os.path.join(ROOT, "." + "venv", "bin", "python")
    srv = subprocess.Popen([py, os.path.join(ROOT, "serve", "server.py"), "--engine", "strata", "--config", cfg_path,
                            "--port", str(PORT)], cwd=ROOT, stdout=srv_log, stderr=subprocess.STDOUT,
                           start_new_session=True)
    t0 = time.time()
    while True:
        if srv.poll() is not None:
            raise RuntimeError(f"server exited ({srv.returncode}); see {srv_log.name}")
        try:
            urllib.request.urlopen(f"http://127.0.0.1:{PORT}/v1/models", timeout=2)
            break
        except Exception:
            if time.time() - t0 > 900:
                raise RuntimeError("server not ready after 900 s")
            time.sleep(2)
    return srv, cfg["log"]


def stop_server(srv):
    os.killpg(srv.pid, signal.SIGTERM)
    try:
        srv.wait(30)
    except subprocess.TimeoutExpired:
        os.killpg(srv.pid, signal.SIGKILL)
        srv.wait()
    for _ in range(60):
        stats = subprocess.run(["ps", "-C", "strata", "-o", "stat="], capture_output=True, text=True).stdout
        if all(s.startswith("Z") for s in stats.split()):
            break
        time.sleep(1)


class GttWatch(threading.Thread):
    """The largest GTT use of each card while the suite runs (the desktop card's spill shows only there)."""
    def __init__(self):
        super().__init__(daemon=True)
        self.files = sorted(f"/sys/class/drm/{d}/device/mem_info_gtt_used" for d in os.listdir("/sys/class/drm")
                            if re.fullmatch(r"card\d+", d) and os.path.exists(f"/sys/class/drm/{d}/device/mem_info_gtt_used"))
        self.base = [self.read(f) for f in self.files]
        self.peak = list(self.base)
        self.stop = False

    @staticmethod
    def read(f):
        try:
            return int(open(f).read())
        except OSError:
            return 0

    mem_min = None   # the lowest MemAvailable seen (GiB)

    def run(self):
        while not self.stop:
            for i, f in enumerate(self.files):
                self.peak[i] = max(self.peak[i], self.read(f))
            try:
                kb = int(next(l for l in open("/proc/meminfo") if l.startswith("MemAvailable")).split()[1])
                gib = round(kb / 2**20, 1)
                self.mem_min = gib if self.mem_min is None else min(self.mem_min, gib)
            except (OSError, StopIteration, ValueError):
                pass
            time.sleep(2)


# ------------------------------------------------------------------ requests
def chat(messages, max_tokens, temperature=0.0, thinking=False, stream=False, timeout=3600, **extra):
    body = {"model": "strata", "messages": messages, "max_tokens": max_tokens, "temperature": temperature,
            "chat_template_kwargs": {"enable_thinking": thinking}, **extra}
    if temperature > 0:
        body.setdefault("top_p", 0.95)
        body.setdefault("seed", 1234)
    body["stream"] = stream
    req = urllib.request.Request(f"http://127.0.0.1:{PORT}/v1/chat/completions", json.dumps(body).encode(),
                                 {"Content-Type": "application/json"})
    t0 = time.time()
    if not stream:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            out = json.load(r)
        m = out["choices"][0]["message"]
        return {"content": m.get("content") or "", "reasoning": m.get("reasoning_content") or "",
                "usage": out.get("usage", {}), "timings": out.get("timings", {}), "wall": time.time() - t0,
                "finish": out["choices"][0].get("finish_reason")}
    content, reasoning, marks, last = [], [], [], {}
    first = None
    with urllib.request.urlopen(req, timeout=timeout) as r:
        for raw in r:
            line = raw.decode("utf-8", "replace").strip()
            if not line.startswith("data:"):
                continue
            data = line[5:].strip()
            if data == "[DONE]":
                break
            x = json.loads(data)
            for key in ("usage", "timings"):
                if x.get(key):
                    last[key] = x[key]
            for ch in x.get("choices") or []:
                d = ch.get("delta") or {}
                piece = d.get("content") or ""
                rpiece = d.get("reasoning_content") or ""
                if piece or rpiece:
                    now = time.time() - t0
                    if first is None:
                        first = now
                    content.append(piece)
                    reasoning.append(rpiece)
                    marks.append(now)
                if ch.get("finish_reason"):
                    last["finish"] = ch["finish_reason"]
    return {"content": "".join(content), "reasoning": "".join(reasoning), "usage": last.get("usage", {}),
            "timings": last.get("timings", {}), "wall": time.time() - t0, "ttft": first, "marks": marks,
            "finish": last.get("finish")}


# ------------------------------------------------------------------ checks
CJK = re.compile(r"[぀-ヿ㐀-䶿一-鿿가-힯]")


def degeneration(text):
    """Signs of garbage or a loop. Returns the measures and `bad` with its reasons."""
    words = text.split()
    r = {"chars": len(text), "words": len(words)}
    if len(text) < 200:
        r.update(bad=False, why=[])
        return r
    seg = 400
    comps = []
    for i in range(0, max(1, len(words) - seg // 2), seg):
        s = " ".join(words[i:i + seg]).encode()
        if len(s) > 500:
            comps.append(len(zlib.compress(s, 9)) / len(s))
    r["comp_min"] = round(min(comps), 3) if comps else None
    r["comp_med"] = round(statistics.median(comps), 3) if comps else None
    grams = collections.Counter(tuple(words[i:i + 8]) for i in range(len(words) - 7))
    r["rep8_max"] = max(grams.values()) if grams else 0
    tail = text[-4000:]
    # a unit repeated 5+ times in a row at the end, not a divider (a run of one character such as "=====")
    loop = next((m for m in re.finditer(r"(.{12,300}?)\1{4,}", tail, re.S) if len(set(m.group(1).strip())) > 2), None)
    r["tail_loop"] = loop.group(1)[:60] if loop else ""
    bad_chars = sum(1 for c in text if c == "�" or (ord(c) < 32 and c not in "\n\t\r"))
    r["bad_char_frac"] = round(bad_chars / len(text), 5)
    letters = sum(1 for c in text if c.isalpha())
    r["cjk_frac"] = round(len(CJK.findall(text)) / max(1, letters), 4)
    why = []
    if r["comp_min"] is not None and r["comp_min"] < 0.12:
        why.append(f"a highly repetitive stretch (compresses to {r['comp_min']})")
    if r["rep8_max"] > 8:
        why.append(f"an 8-word sequence repeated {r['rep8_max']} times")
    if r["tail_loop"]:
        why.append(f"the end loops on {r['tail_loop']!r}")
    if r["bad_char_frac"] > 0.002:
        why.append(f"broken characters {r['bad_char_frac']}")
    if r["cjk_frac"] > 0.02:
        why.append(f"CJK text in an English answer ({r['cjk_frac']})")
    r.update(bad=bool(why), why=why)
    return r


def stream_speed(marks, timings, n_seg=4):
    """Decode speed in n_seg parts of the answer (by streamed pieces), from the first piece on."""
    if not marks or len(marks) < 8:
        return []
    n_tok = (timings or {}).get("predicted_n") or len(marks)
    per_piece = n_tok / len(marks)
    out = []
    k = len(marks) // n_seg
    for s in range(n_seg):
        a, b = s * k, (s + 1) * k - 1 if s < n_seg - 1 else len(marks) - 1
        dt = marks[b] - marks[a]
        out.append(round((b - a) * per_piece / dt, 1) if dt > 0 else None)
    return out


def text_of(tokens, offset):
    """~`tokens` of the repository's text, from character `offset` (a different start defeats the prompt cache)."""
    n = int(tokens * CHARS_PER_TOKEN)
    big = haystack(n + offset)
    return big[offset:offset + n]


def kt(s):
    return int(float(s.lower().rstrip("k")) * 1024) if s.lower().endswith("k") else int(s)


# ------------------------------------------------------------------ the tests
class Suite:
    def __init__(self, a, out, log):
        self.a, self.out, self.log = a, out, log
        self.rows = []
        os.makedirs(os.path.join(out, "answers"), exist_ok=True)
        self.f = open(os.path.join(out, "results.jsonl"), "a")
        self.rnd = random.Random(20261009)

    def keep(self, row, answer=None):
        row["label"] = self.a.label
        row["at"] = time.strftime("%H:%M:%S")
        if answer is not None:
            name = f"{row['test']}-{row.get('name', '')}".replace("/", "_")
            with open(os.path.join(self.out, "answers", name + ".txt"), "w") as f:
                f.write(answer)
        self.rows.append(row)
        self.f.write(json.dumps(row) + "\n")
        self.f.flush()
        print(json.dumps({k: v for k, v in row.items() if k not in ("why",)})[:400], flush=True)

    def speed(self):
        for L in self.a.speed_sizes.split(","):
            tok = kt(L)
            body = f"Session {self.rnd.randint(10**8, 10**9)}.\n" + text_of(tok, offset=self.rnd.randint(0, 400000))
            r = chat([{"role": "user", "content": body + "\n\nSummarize the text above in three sentences."}], 200)
            t = r["timings"]
            self.keep({"test": "speed", "name": L, "prompt_n": t.get("prompt_n"), "prompt_tps": t.get("prompt_per_second"),
                       "ttft_s": round((t.get("prompt_ms") or 0) / 1000, 1), "decode_n": t.get("predicted_n"),
                       "decode_tps": t.get("predicted_per_second"), "deg": degeneration(r["content"])}, r["content"])

    def needles(self):
        p = os.path.join(self.out, "needles.json")
        py = os.path.join(ROOT, "." + "venv", "bin", "python")
        subprocess.run([py, os.path.join(ROOT, "tools", "needle_bench.py"), "--url", f"http://127.0.0.1:{PORT}",
                        "--lengths", self.a.needle_sizes, "--depths", "10,50,90", "--out", p], cwd=ROOT)
        try:
            res = json.load(open(p))
        except (OSError, ValueError):
            res = []
        for x in res if isinstance(res, list) else res.get("rows", []):
            self.keep({"test": "needle", "name": f"{x.get('length')}@{x.get('depth')}", **x})

    def facts(self):
        for L in self.a.fact_sizes.split(","):
            tok = kt(L)
            text = text_of(tok, offset=self.rnd.randint(0, 400000))
            alpha = self.rnd.randint(1000, 4999)
            plus = self.rnd.randint(1000, 4999)
            courier = self.rnd.choice(["Teodora Vlasic", "Ilinca Moraru", "Bogdan Serban", "Mirela Toma"])
            facts = [(0.2, f"Note for the auditors: the access code of vault ALPHA is {alpha}."),
                     (0.55, f"Note for the auditors: the access code of vault BETA is the code of vault ALPHA plus {plus}."),
                     (0.85, f"Note for the auditors: the courier assigned to vault GAMMA is {courier}.")]
            for depth, line in reversed(facts):
                cut = text.rfind("\n", 0, int(len(text) * depth)) + 1
                text = text[:cut] + "\n" + line + "\n" + text[cut:]
            q = ("\n\nThe text above contains three notes for the auditors. Using them: (1) what is the access code of "
                 "vault BETA? (2) who is the courier assigned to vault GAMMA? Answer briefly.")
            r = chat([{"role": "user", "content": text + q}], 4000, thinking=True)
            want = str(alpha + plus)
            ok_code, ok_name = want in r["content"], courier.split()[0] in r["content"]
            t = r["timings"]
            self.keep({"test": "facts", "name": L, "prompt_n": t.get("prompt_n"), "ok": ok_code and ok_name,
                       "ok_code": ok_code, "ok_name": ok_name, "want": f"{want} / {courier}",
                       "answer": r["content"][-300:], "reasoning_chars": len(r["reasoning"]),
                       "deg": degeneration(r["reasoning"] + "\n" + r["content"]), "prompt_tps": t.get("prompt_per_second"),
                       "decode_tps": t.get("predicted_per_second")},
                      "=== reasoning ===\n" + r["reasoning"] + "\n=== answer ===\n" + r["content"])

    def longans(self):
        ask = ("\n\nBased on the files above, write a detailed technical report (around 3000 words) on what this software "
               "does, how its main parts fit together, and notable design decisions. Use headings and keep going "
               "until the report is complete.")
        for L in self.a.longans_sizes.split(","):
            tok = kt(L)
            text = f"Session {self.rnd.randint(10**8, 10**9)}.\n" + text_of(tok, offset=self.rnd.randint(0, 400000))
            r = chat([{"role": "user", "content": text + ask}], self.a.longans_tokens, stream=True)
            t = r["timings"]
            self.keep({"test": "longans", "name": L, "prompt_n": t.get("prompt_n") or r["usage"].get("prompt_tokens"),
                       "prompt_tps": t.get("prompt_per_second"), "ttft_s": round(r["ttft"] or 0, 1),
                       "decode_n": t.get("predicted_n") or r["usage"].get("completion_tokens"),
                       "decode_tps": t.get("predicted_per_second"), "decode_by_quarter": stream_speed(r["marks"], t),
                       "finish": r["finish"], "deg": degeneration(r["content"])}, r["content"])

    def longgen(self):
        tasks = [("story", "Write a long, detailed story (at least 12000 words) about a lighthouse keeper on a remote "
                           "island over one year. Write it in full, chapter by chapter, with dialogue."),
                 ("code", "Write a complete, well-commented Python program for a small library management system: data "
                          "model, a SQLite store, a command-line interface with every command, input validation, and a "
                          "full set of unit tests. Write all of the code in full, file by file.")]
        for name, prompt in tasks:
            for temp in (0.0, 0.7):
                # no tools in this request: say so (sampled, the model may otherwise start an agent's tool call -
                # the dry run's code task did, and the server returned it as a tool call)
                r = chat([{"role": "system", "content": "You have no tools. Answer directly with text only."},
                          {"role": "user", "content": prompt}], self.a.longgen_tokens, temperature=temp, stream=True)
                t = r["timings"]
                self.keep({"test": "longgen", "name": f"{name}-t{temp}",
                           "decode_n": t.get("predicted_n") or r["usage"].get("completion_tokens"),
                           "decode_tps": t.get("predicted_per_second"), "decode_by_quarter": stream_speed(r["marks"], t),
                           "finish": r["finish"], "deg": degeneration(r["content"])}, r["content"])

    def turns(self):
        names = ["HELIOTROPE", "BASALT", "MARZIPAN", "KESTREL"]
        part = kt(self.a.turn_part)
        msgs = []
        for i, code in enumerate(names):
            text = text_of(part, offset=200000 * (i + 1) + self.rnd.randint(0, 50000))
            cut = text.rfind("\n", 0, int(len(text) * self.rnd.uniform(0.2, 0.8))) + 1
            text = text[:cut] + f"\n(Reminder: the project codename for part {i + 1} is {code}.)\n" + text[cut:]
            msgs.append({"role": "user", "content": f"Here is part {i + 1} of a document collection.\n\n{text}\n\n"
                                                    f"Reply with one short sentence confirming you received part {i + 1}."})
            r = chat(msgs, 60)
            msgs.append({"role": "assistant", "content": r["content"]})
            t = r["timings"]
            self.keep({"test": "turns", "name": f"part{i + 1}", "prompt_n": t.get("prompt_n"),
                       "prompt_tps": t.get("prompt_per_second"), "ttft_s": round((t.get("prompt_ms") or 0) / 1000, 1),
                       "total_tokens": r["usage"].get("prompt_tokens"), "reply": r["content"][:120]})
        msgs.append({"role": "user", "content": "List the project codenames of parts 1, 2, 3 and 4, in order, one per line."})
        r = chat(msgs, 300)   # thinking off like the turns before it: the conversation cache's path
        found = [n for n in names if n in r["content"].upper()]
        order = [r["content"].upper().find(n) for n in found]
        t = r["timings"]
        self.keep({"test": "turns", "name": "recall", "total_tokens": r["usage"].get("prompt_tokens"),
                   "prompt_n": t.get("prompt_n"), "ok": len(found) == 4 and order == sorted(order),
                   "found": found, "answer": r["content"][-300:], "deg": degeneration(r["reasoning"] + r["content"])},
                  "=== reasoning ===\n" + r["reasoning"] + "\n=== answer ===\n" + r["content"])


def _par_one(suite, i, tok, out_rows, t_start):
    rnd = random.Random(1000 + i)
    text = f"Conversation {i}, session {rnd.randint(10**8, 10**9)}.\n" + text_of(tok, offset=120000 * i + rnd.randint(0, 40000))
    alpha, plus = rnd.randint(1000, 4999), rnd.randint(1000, 4999)
    courier = ["Teodora Vlasic", "Ilinca Moraru", "Bogdan Serban", "Mirela Toma", "Radu Enache", "Sorina Pop",
               "Victor Ionescu", "Dana Lupu"][i % 8]
    delta = rnd.randint(100, 999)
    facts = [(0.2, f"Note for the auditors: the access code of vault ALPHA is {alpha}."),
             (0.5, f"Note for the auditors: the access code of vault BETA is the code of vault ALPHA plus {plus}."),
             (0.7, f"Note for the auditors: the shelf number of vault DELTA is {delta}."),
             (0.85, f"Note for the auditors: the courier assigned to vault GAMMA is {courier}.")]
    for depth, line in reversed(facts):
        cut = text.rfind("\n", 0, int(len(text) * depth)) + 1
        text = text[:cut] + "\n" + line + "\n" + text[cut:]
    q = ("\n\nThe text above contains notes for the auditors. First answer: (1) the access code of vault BETA, (2) the "
         "courier of vault GAMMA. Then write a summary (around 1000 words) of what the files above are about.")
    msgs = [{"role": "user", "content": text + q}]
    t0 = time.time()
    r = chat(msgs, suite.a.par_tokens, stream=True)
    t1 = time.time()
    t = r["timings"]
    want = str(alpha + plus)
    row = {"test": "par", "name": f"c{i}", "conv": i, "prompt_n": t.get("prompt_n") or r["usage"].get("prompt_tokens"),
           "start_s": round(t0 - t_start, 1), "ttft_s": round(r["ttft"] or 0, 1), "end_s": round(t1 - t_start, 1),
           "decode_n": t.get("predicted_n") or r["usage"].get("completion_tokens"), "decode_tps": t.get("predicted_per_second"),
           "ok_code": want in r["content"], "ok_name": courier.split()[0] in r["content"],
           "want": f"{want} / {courier}", "codes": (want, courier, str(delta)), "finish": r["finish"],
           "deg": degeneration(r["content"])}
    # a second turn in the same conversation: its slot's cache (the next turn of a conversation goes to its slot)
    msgs += [{"role": "assistant", "content": r["content"]},
             {"role": "user", "content": "What is the shelf number of vault DELTA in the notes above? Reply with the number only."}]
    r2 = chat(msgs, 30)
    t2 = r2["timings"]
    row.update(turn2_ok=str(delta) in r2["content"], turn2_answer=r2["content"][:40], turn2_read=t2.get("prompt_n"),
               turn2_ttft_s=round((t2.get("prompt_ms") or 0) / 1000, 1))
    out_rows.append((row, r["content"]))


def _par(self):
    tok = kt(self.a.par_size)
    rows, threads = [], []
    t_start = time.time()
    for i in range(self.a.par_n):
        th = threading.Thread(target=_par_one, args=(self, i, tok, rows, t_start))
        th.start()
        threads.append(th)
    for th in threads:
        th.join()
    rows.sort(key=lambda x: x[0]["conv"])
    mine = {r["conv"]: r["codes"] for r, _ in rows}
    for r, content in rows:   # another conversation's code or courier in this answer: the slots mixed up
        others = [c for j, cs in mine.items() if j != r["conv"] for c in cs if c in content]
        r["crosstalk"] = others
        r["ok"] = r["ok_code"] and r["ok_name"] and r["turn2_ok"] and not others and not r["deg"]["bad"]
        self.keep(r, content)
    if rows:
        first = min(r["start_s"] + r["ttft_s"] for r, _ in rows)
        last = max(r["end_s"] for r, _ in rows)
        tot = sum(r["decode_n"] or 0 for r, _ in rows)
        self.keep({"test": "par", "name": "all", "conversations": len(rows), "decode_tokens": tot,
                   "first_token_s": round(first, 1), "last_end_s": round(last, 1),
                   "decode_tps_total": round(tot / max(1e-9, last - first), 1),
                   "all_ok": all(r["ok"] for r, _ in rows)})


Suite.par = _par


def _convcache(self):
    """K long conversations, then rounds of follow-ups alternating between them (the single prompt cache's worst case):
    tokens re-read, time to the first token and the answer of each follow-up. Run it with and without
    --conversation-cache-mib to compare; the follow-ups' text is kept to compare the two runs token for token."""
    K, tok, R = self.a.cc_n, kt(self.a.cc_size), self.a.cc_rounds
    convs = []
    for i in range(K):
        rnd = random.Random(500 + i)
        text = f"Conversation {i}, session {rnd.randint(10**8, 10**9)}.\n" + text_of(tok, offset=150000 * i + rnd.randint(0, 40000))
        f = {"ALPHA": str(rnd.randint(1000, 4999)), "DELTA": str(rnd.randint(100, 999)),
             "GAMMA": ["Teodora Vlasic", "Ilinca Moraru", "Bogdan Serban", "Mirela Toma"][i % 4],
             "EPSILON": rnd.choice(["violet", "saffron", "indigo", "crimson", "amber"])}
        notes = [(0.15, f"Note for the auditors: the access code of vault ALPHA is {f['ALPHA']}."),
                 (0.4, f"Note for the auditors: the shelf number of vault DELTA is {f['DELTA']}."),
                 (0.65, f"Note for the auditors: the courier assigned to vault GAMMA is {f['GAMMA']}."),
                 (0.9, f"Note for the auditors: the colour of vault EPSILON's door is {f['EPSILON']}.")]
        for depth, line in reversed(notes):
            cut = text.rfind("\n", 0, int(len(text) * depth)) + 1
            text = text[:cut] + "\n" + line + "\n" + text[cut:]
        msgs = [{"role": "user", "content": text + "\n\nRead the text above. Reply with one short sentence saying what it is about."}]
        r = chat(msgs, 80)
        msgs.append({"role": "assistant", "content": r["content"]})
        t = r["timings"]
        self.keep({"test": "convcache", "name": f"c{i}-open", "conv": i, "prompt_n": t.get("prompt_n"),
                   "ttft_s": round((t.get("prompt_ms") or 0) / 1000, 1), "answer": r["content"][:100]})
        convs.append((msgs, f))
    qs = [("GAMMA", "Who is the courier assigned to vault GAMMA in the notes above? Reply with the name only."),
          ("DELTA", "What is the shelf number of vault DELTA in the notes above? Reply with the number only."),
          ("ALPHA", "What is the access code of vault ALPHA in the notes above? Reply with the number only."),
          ("EPSILON", "What colour is vault EPSILON's door in the notes above? Reply with the colour only.")]
    for rr in range(R):
        for i, (msgs, f) in enumerate(convs):
            key, q = qs[(rr + i) % len(qs)]
            msgs.append({"role": "user", "content": q})
            r = chat(msgs, 30)
            msgs.append({"role": "assistant", "content": r["content"]})
            t = r["timings"]
            want = f[key].split()[0]
            self.keep({"test": "convcache", "name": f"r{rr}-c{i}-{key}", "conv": i, "round": rr,
                       "total_tokens": r["usage"].get("prompt_tokens"), "prompt_n": t.get("prompt_n"),
                       "ttft_s": round((t.get("prompt_ms") or 0) / 1000, 2), "wall_s": round(r["wall"], 2),
                       "ok": want.lower() in r["content"].lower(), "want": want, "answer": r["content"][:60]})


Suite.convcache = _convcache


def report(out, label, rows, gtt, errors, log_tail):
    L = [f"# Long-context suite: {label} ({time.strftime('%Y-%m-%d %H:%M')})", ""]
    L.append(f"GTT peak per card (MiB, before -> peak): " +
             ", ".join(f"{b // 2**20} -> {p // 2**20}" for b, p in gtt) + f"; engine error lines: {errors}")
    L.append("")
    def deg(r):
        d = r.get("deg") or {}
        return "BAD: " + "; ".join(d.get("why", [])) if d.get("bad") else (f"ok (comp {d.get('comp_min')}, rep8 {d.get('rep8_max')})" if d else "")
    for test, cols in [("speed", ["name", "prompt_n", "prompt_tps", "ttft_s", "decode_tps"]),
                       ("needle", ["name", "found", "answer", "prompt_tokens", "seconds"]),
                       ("facts", ["name", "prompt_n", "ok", "want", "decode_tps"]),
                       ("longans", ["name", "prompt_n", "ttft_s", "decode_n", "decode_tps", "decode_by_quarter", "finish"]),
                       ("longgen", ["name", "decode_n", "decode_tps", "decode_by_quarter", "finish"]),
                       ("turns", ["name", "total_tokens", "prompt_n", "ttft_s", "ok", "found"]),
                       ("par", ["name", "prompt_n", "start_s", "ttft_s", "end_s", "decode_n", "decode_tps", "want",
                                "ok_code", "ok_name", "turn2_ok", "turn2_read", "crosstalk", "ok", "decode_tps_total",
                                "all_ok"]),
                       ("convcache", ["name", "total_tokens", "prompt_n", "ttft_s", "wall_s", "ok", "want", "answer"])]:
        rs = [r for r in rows if r["test"] == test]
        if not rs:
            continue
        L += [f"## {test}", "", "| " + " | ".join(cols + ["degeneration"]) + " |", "|" + "---|" * (len(cols) + 1)]
        for r in rs:
            L.append("| " + " | ".join(str(r.get(c, ""))[:80].replace("|", "/").replace("\n", " ") for c in cols) +
                     f" | {deg(r)} |")
        L.append("")
    L += ["## engine log (last lines)", "", "```", *log_tail, "```"]
    open(os.path.join(out, "report.md"), "w").write("\n".join(L) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--out", default=None)
    ap.add_argument("--exe", default=None)
    ap.add_argument("--arg", action="append", default=[])
    ap.add_argument("--drop-arg", action="append", default=[])
    ap.add_argument("--env", action="append", default=[])
    ap.add_argument("--set", action="append", default=[], help="a config key: --set parallel=4 (the value as JSON)")
    ap.add_argument("--par-n", type=int, default=4, help="par: conversations at once")
    ap.add_argument("--par-size", default="64k", help="par: each conversation's prompt")
    ap.add_argument("--par-tokens", type=int, default=2500)
    ap.add_argument("--cc-n", type=int, default=3, help="convcache: conversations")
    ap.add_argument("--cc-size", default="64k", help="convcache: each conversation's first prompt")
    ap.add_argument("--cc-rounds", type=int, default=3, help="convcache: rounds of follow-ups over all conversations")
    ap.add_argument("--tests", default="speed,needles,facts,longans,longgen,turns")
    ap.add_argument("--speed-sizes", default="4k,16k,32k,64k,118k")
    ap.add_argument("--needle-sizes", default="8k,32k,64k,128k")
    ap.add_argument("--fact-sizes", default="32k,64k,118k")
    ap.add_argument("--longans-sizes", default="32k,64k,118k")
    ap.add_argument("--longans-tokens", type=int, default=4096)
    ap.add_argument("--longgen-tokens", type=int, default=16000)
    ap.add_argument("--turn-part", default="24k")
    a = ap.parse_args()
    out = a.out or os.path.expanduser(f"~/strata-tools/longctx/{a.label}")
    os.makedirs(out, exist_ok=True)
    gw = GttWatch()
    gw.start()
    srv, log = start_server(a, out)
    s = Suite(a, out, log)
    try:
        for t in a.tests.split(","):
            print(f"== {t}", flush=True)
            try:
                getattr(s, t)()
            except Exception as e:  # one test's failure is a result, the others still run
                s.keep({"test": t, "name": "error", "error": repr(e)[:500]})
    finally:
        stop_server(srv)
        gw.stop = True
    lines = open(log, errors="replace").read().splitlines()
    errors = [l for l in lines if re.search(r"\bERR\b|error:|\bfault\b|\bfailed\b|timed out", l, re.I) and "no error" not in l]
    s.keep({"test": "system", "name": "ram", "mem_available_min_gib": gw.mem_min})
    report(out, a.label, s.rows, list(zip(gw.base, gw.peak)), len(errors), (errors[-10:] or lines[-12:]))
    print(f"report: {os.path.join(out, 'report.md')}")


if __name__ == "__main__":
    main()
