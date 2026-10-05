#!/usr/bin/env python3
"""Server smoke for the installed config (2x RX 7900 XTX, IQ3_XXS) with the current engine.

Starts serve/server.py (engine log to data/logs/smoke-*.engine.txt), then: a fact question, a 4.5K-token prompt
(a summary of docs/AMD_HIP.md), the Anthropic endpoint, a stream cut off after 3 s followed by a new request, a
two-turn chat and a sampled answer.  Prints each answer's start and speed, and the engine log's error lines.
"""
import json, os, signal, subprocess, sys, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PORT = 8091


def call(path, body, headers=None, timeout=900):
    req = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", json.dumps(body).encode(),
                                 {"Content-Type": "application/json", **(headers or {})})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def chat(name, body, check):
    r = call("/v1/chat/completions", body)
    m, t = r["choices"][0]["message"], r["timings"]
    text = m.get("content") or ""
    ok = check(text)
    print(f"{'PASS' if ok else 'FAIL'} {name}: {text[:150]!r} | prompt {t['prompt_n']} @ {t['prompt_per_second']} "
          f"tok/s, decode {t['predicted_n']} @ {t['predicted_per_second']} tok/s")
    return ok


def main():
    cfg = json.load(open(os.path.join(ROOT, "strata-iq3_xxs.json")))
    logs = os.path.join(HERE, "data", "logs")
    os.makedirs(logs, exist_ok=True)
    stamp = time.strftime("%H%M%S")
    cfg["log"] = os.path.join(logs, f"smoke-{stamp}.engine.txt")
    cfg_path = os.path.join(logs, f"smoke-{stamp}.config.json")
    json.dump(cfg, open(cfg_path, "w"), indent=1)
    srv = subprocess.Popen([os.path.join(ROOT, ".venv", "bin", "python"), os.path.join(ROOT, "serve", "server.py"),
                            "--engine", "strata", "--config", cfg_path, "--port", str(PORT)], cwd=ROOT,
                           stdout=open(os.path.join(logs, f"smoke-{stamp}.server.txt"), "w"),
                           stderr=subprocess.STDOUT, start_new_session=True)
    results = []
    try:
        t0 = time.time()
        while True:
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{PORT}/v1/models", timeout=2)
                break
            except Exception:
                if srv.poll() is not None or time.time() - t0 > 900:
                    print("FAIL: server did not start"); return 1
                time.sleep(2)
        results.append(chat("facts", {"messages": [{"role": "user", "content": "What is the capital of Australia, and "
                             "what is 17*23? Answer in one short line."}], "max_tokens": 800, "temperature": 0},
                            lambda s: "Canberra" in s and "391" in s))
        doc = open(os.path.join(ROOT, "docs", "AMD_HIP.md")).read()[:15000]
        results.append(chat("4.5K prompt", {"messages": [{"role": "user", "content": "Summarize this document in 5 "
                             "bullet points:\n\n" + doc}], "max_tokens": 3000, "temperature": 0},
                            lambda s: "HIP" in s and s.count("- ") >= 3))
        r = call("/v1/messages", {"model": "x", "max_tokens": 400, "messages": [{"role": "user", "content":
                 "Name three primary colors, comma-separated, nothing else."}]}, {"anthropic-version": "2023-06-01"})
        text = " ".join(c.get("text", "") for c in r["content"] if c["type"] == "text")
        ok = "red" in text.lower() and r["stop_reason"] == "end_turn"
        print(f"{'PASS' if ok else 'FAIL'} anthropic: {text!r} ({r['stop_reason']})")
        results.append(ok)
        # a stream cut off mid-generation, then a new request must answer normally
        req = urllib.request.Request(f"http://127.0.0.1:{PORT}/v1/chat/completions", json.dumps(
            {"messages": [{"role": "user", "content": "Count from 1 to 500, one number per line."}],
             "max_tokens": 2000, "stream": True}).encode(), {"Content-Type": "application/json"})
        chunks, t1 = 0, time.time()
        with urllib.request.urlopen(req, timeout=60) as s:
            for line in s:
                chunks += line.startswith(b"data:")
                if time.time() - t1 > 3:
                    break
        print(f"{'PASS' if chunks > 10 else 'FAIL'} stream: {chunks} chunks before the client left")
        results.append(chunks > 10)
        results.append(chat("after cancel", {"messages": [{"role": "user", "content": "Write a haiku about GPUs."}],
                            "max_tokens": 600, "temperature": 0}, lambda s: len(s.split()) >= 5))
        results.append(chat("two turns", {"messages": [{"role": "user", "content": "My name is Ana. Remember it."},
                            {"role": "assistant", "content": "Got it, Ana."},
                            {"role": "user", "content": "What is my name? One word."}],
                            "max_tokens": 400, "temperature": 0}, lambda s: "Ana" in s))
        results.append(chat("sampled", {"messages": [{"role": "user", "content": "Give me a random fruit name, one "
                            "word."}], "max_tokens": 300, "temperature": 0.8, "seed": 7}, lambda s: len(s) > 0))
    finally:
        os.killpg(srv.pid, signal.SIGTERM)
        try:
            srv.wait(30)
        except subprocess.TimeoutExpired:
            os.killpg(srv.pid, signal.SIGKILL)
    bad = [l.strip() for l in open(cfg["log"]) if any(w in l.lower() for w in ("error", "timed out", "never rang"))
           and "no error" not in l]
    print(f"engine log: {len(bad)} error lines" + ("".join("\n  " + l[:200] for l in bad[:5])))
    print(f"{sum(results)}/{len(results)} passed")
    return 0 if all(results) and not bad else 1


if __name__ == "__main__":
    sys.exit(main())
