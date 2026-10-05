#!/usr/bin/env python3
"""Several clients at once against one engine configuration (2x RX 7900 XTX, IQ3_XXS).

Starts serve/server.py with a variant of the installed config, sends one warm-up, then for each concurrency C
(--clients, e.g. 1,2,4) runs --rounds rounds of C streamed requests started together (different agent-style tasks,
greedy, --tokens each).  Per round: total tok/s (all completion tokens / time until the last one finished), each
request's tok/s and time to its first token.  Prints one JSON line per configuration (also appended to --out).

  mbench.py --label solo                                    # the installed config: one request at a time
  mbench.py --label b2g2 --layer-split 26 --arg=--batch --arg=2 --arg=--batch-groups --arg=2 --arg=--trim-stage-weights
"""
import argparse, json, os, signal, statistics, subprocess, sys, threading, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
TASKS = [
    "Write a Python function that merges overlapping intervals, with tests.",
    "Explain how a B-tree insertion works, step by step, with a small example.",
    "Write a bash script that finds the 10 largest files under a directory and prints them human-readably.",
    "Review this idea: caching HTTP responses in a dict keyed by URL inside a Flask app. List the problems.",
    "Write a TypeScript debounce function with proper types and a usage example.",
    "Compare TCP and QUIC: handshake, congestion control, head-of-line blocking.",
    "Write a SQL query that returns each customer's three most recent orders, and explain it.",
    "Describe how you would structure unit tests for a rate limiter class.",
    "Write a Rust function that parses 'key=value;key2=value2' into a HashMap, with error handling.",
    "Explain the difference between a mutex and a semaphore with a concrete example.",
    "Write a Python async function that fetches 20 URLs with at most 5 in flight.",
    "Explain what a git rebase does and when not to use it.",
]


def stream_request(port, text, tokens, out):
    body = {"messages": [{"role": "user", "content": text}], "max_tokens": tokens, "temperature": 0, "stream": True}
    req = urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions", json.dumps(body).encode(),
                                 {"Content-Type": "application/json"})
    t0 = time.time()
    first, usage = None, None
    try:
        with urllib.request.urlopen(req, timeout=1800) as r:
            for line in r:
                if not line.startswith(b"data:") or line.strip() == b"data: [DONE]":
                    continue
                d = json.loads(line[5:])
                delta = (d.get("choices") or [{}])[0].get("delta") or {}
                if first is None and (delta.get("content") or delta.get("reasoning_content")):
                    first = time.time()   # the first token (the role-only delta comes at once)
                if d.get("usage"):
                    usage = d["usage"]
        end = time.time()
        n = usage["completion_tokens"] if usage else 0
        out.update(start=t0, end=end, ttft=(first or end) - t0, n=n, tok_s=n / max(end - (first or t0), 1e-6))
    except Exception as e:
        out.update(error=str(e))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--config", default=os.path.join(ROOT, "strata-iq3_xxs.json"))
    ap.add_argument("--layer-split", default=None)
    ap.add_argument("--env", action="append", default=[])
    ap.add_argument("--arg", action="append", default=[])
    ap.add_argument("--exe", default=None)
    ap.add_argument("--set", action="append", default=[], help='a config key: --set parallel=8 (the value as JSON)')
    ap.add_argument("--clients", default="1,2,4")
    ap.add_argument("--rounds", type=int, default=2)
    ap.add_argument("--tokens", type=int, default=512)
    ap.add_argument("--port", type=int, default=8092)
    ap.add_argument("--out", default=os.path.join(HERE, "data", "results.jsonl"))
    a = ap.parse_args()

    cfg = json.load(open(a.config))
    cfg["args"] = list(cfg["args"]) + a.arg
    if a.exe:
        cfg["exe"] = a.exe
    if a.layer_split is not None:
        cfg["layer_split"] = a.layer_split
    cfg["env"] = {**cfg.get("env", {}), **dict(e.split("=", 1) for e in a.env)}
    for kv in a.set:
        k, v = kv.split("=", 1)
        cfg[k] = json.loads(v)
    logs = os.path.join(HERE, "data", "logs")
    os.makedirs(logs, exist_ok=True)
    stamp = time.strftime("%H%M%S")
    cfg["log"] = os.path.join(logs, f"{a.label}-{stamp}.engine.txt")
    cfg_path = os.path.join(logs, f"{a.label}-{stamp}.config.json")
    json.dump(cfg, open(cfg_path, "w"), indent=1)
    srv_log = os.path.join(logs, f"{a.label}-{stamp}.server.txt")
    srv = subprocess.Popen([os.path.join(ROOT, ".venv", "bin", "python"), os.path.join(ROOT, "serve", "server.py"),
                            "--engine", "strata", "--config", cfg_path, "--port", str(a.port)], cwd=ROOT,
                           stdout=open(srv_log, "w"), stderr=subprocess.STDOUT, start_new_session=True)
    res = {"label": a.label, "arg": a.arg, "env": a.env, "layer_split": a.layer_split, "log": cfg["log"], "rounds": []}
    try:
        t0 = time.time()
        while True:
            if srv.poll() is not None:
                raise RuntimeError(f"server exited; see {srv_log}")
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{a.port}/v1/models", timeout=2)
                break
            except Exception:
                if time.time() - t0 > 900:
                    raise RuntimeError("server not ready after 900 s")
                time.sleep(2)
        try:
            st = json.load(urllib.request.urlopen(f"http://127.0.0.1:{a.port}/v1/status", timeout=5))
            res["serving"] = (st.get("concurrency") or {}).get("serving")
        except Exception:
            pass
        warm = {}
        stream_request(a.port, "Say hi.", 32, warm)
        task = 0
        for c in [int(x) for x in a.clients.split(",")]:
            for rnd in range(a.rounds):
                outs = [{} for _ in range(c)]
                th = [threading.Thread(target=stream_request, args=(a.port, TASKS[(task + i) % len(TASKS)],
                                                                   a.tokens, outs[i])) for i in range(c)]
                task += c
                for t in th:
                    t.start()
                for t in th:
                    t.join()
                if any("error" in o for o in outs):
                    res["rounds"].append({"clients": c, "round": rnd, "error": [o.get("error") for o in outs]})
                    continue
                start, end = min(o["start"] for o in outs), max(o["end"] for o in outs)
                res["rounds"].append({"clients": c, "round": rnd, "total_tok_s": round(sum(o["n"] for o in outs) / (end - start), 1),
                                      "per_req_tok_s": [round(o["tok_s"], 1) for o in outs],
                                      "ttft_s": [round(o["ttft"], 2) for o in outs], "tokens": [o["n"] for o in outs]})
        summ = {}
        for c in sorted({r["clients"] for r in res["rounds"]}):
            rs = [r for r in res["rounds"] if r["clients"] == c and "error" not in r]
            if rs:
                summ[str(c)] = {"total": statistics.median(r["total_tok_s"] for r in rs),
                                "per_req": statistics.median(x for r in rs for x in r["per_req_tok_s"]),
                                "ttft_max": statistics.median(max(r["ttft_s"]) for r in rs)}
        res["summary"] = summ
    except Exception as e:
        res["error"] = str(e)
    finally:
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
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "a") as f:
        f.write(json.dumps(res) + "\n")
    print(json.dumps({"label": a.label, "serving": res.get("serving"), "summary": res.get("summary"),
                      "error": res.get("error")}))
    return 1 if "error" in res else 0


if __name__ == "__main__":
    sys.exit(main())
