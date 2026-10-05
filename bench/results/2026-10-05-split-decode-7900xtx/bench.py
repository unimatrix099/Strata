#!/usr/bin/env python3
"""Decode-speed bench for one engine configuration (2x RX 7900 XTX, IQ3_XXS).

Starts serve/server.py with a variant of the installed config, sends one warm-up and then a story and a code prompt
REPS times (greedy, 512 tokens), records decode tok/s, draft acceptance and a hash of every answer, stops the server
and prints one JSON line (also appended to --out).

  bench.py --label base                      # the installed config (--gpus 1,0)
  bench.py --label one --gpus 1              # HIP device 1 alone
  bench.py --label x --env STRATA_FOO=1 --arg --some-flag --arg 3
"""
import argparse, hashlib, json, os, signal, statistics, subprocess, sys, time, urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
PROMPTS = {
    "story": "Write a short story (about 400 words) about a lighthouse keeper who finds a message in a bottle.",
    "code": "Write a Python implementation of an LRU cache class with get and put methods, O(1) each, with "
            "docstrings and a short usage example.",
}


def post(port, body, timeout=900):
    req = urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions", json.dumps(body).encode(),
                                 {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--config", default=os.path.join(ROOT, "strata-iq3_xxs.json"))
    ap.add_argument("--gpus", default=None, help="'1,0' (split) or '1' (one card); default: the config's")
    ap.add_argument("--env", action="append", default=[])
    ap.add_argument("--arg", action="append", default=[], help="extra engine argument (repeat for each token)")
    ap.add_argument("--drop-arg", action="append", default=[], help="remove this engine flag (and its value)")
    ap.add_argument("--layer-split", default=None, help="the config's layer_split (\"auto\", \"20\", ...)")
    ap.add_argument("--prefill", action="store_true", help="measure prompt reading instead: a ~4.6K-token prompt "
                    "(docs/AMD_HIP.md), made unique per rep so no prompt cache is reused, 1 token generated")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--exe", default=None, help="run this instead of the config's engine (e.g. a profiler wrapper)")
    ap.add_argument("--tokens", type=int, default=512)
    ap.add_argument("--port", type=int, default=8090)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "data", "results.jsonl"))
    a = ap.parse_args()

    cfg = json.load(open(a.config))
    args = list(cfg["args"])
    for flag in a.drop_arg:
        if flag in args:
            i = args.index(flag)
            has_val = i + 1 < len(args) and not args[i + 1].startswith("--")
            del args[i:i + (2 if has_val else 1)]
    cfg["args"] = args + a.arg
    if a.exe:
        cfg["exe"] = a.exe
    if a.gpus is not None:
        g = [int(x) for x in a.gpus.split(",")]
        if len(g) == 1:
            cfg["gpu"] = g[0]
            for k in ("gpus_asked", "layer_split"):
                cfg.pop(k, None)
            if "--remote-expert-opt" in cfg["args"]:
                cfg["args"].remove("--remote-expert-opt")
        else:
            cfg["gpu"] = g
    if a.layer_split is not None:
        cfg["layer_split"] = a.layer_split
    cfg["env"] = {**cfg.get("env", {}), **dict(e.split("=", 1) for e in a.env)}
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    logdir = os.path.join(os.path.dirname(a.out), "logs")
    os.makedirs(logdir, exist_ok=True)
    stamp = time.strftime("%H%M%S")
    cfg["log"] = os.path.join(logdir, f"{a.label}-{stamp}.engine.txt")
    tmp_cfg = os.path.join(logdir, f"{a.label}-{stamp}.config.json")
    json.dump(cfg, open(tmp_cfg, "w"), indent=1)

    srv_log = open(os.path.join(logdir, f"{a.label}-{stamp}.server.txt"), "w")
    srv = subprocess.Popen([os.path.join(ROOT, ".venv", "bin", "python"), os.path.join(ROOT, "serve", "server.py"),
                            "--engine", "strata", "--config", tmp_cfg, "--port", str(a.port)],
                           cwd=ROOT, stdout=srv_log, stderr=subprocess.STDOUT, start_new_session=True)
    res = {"label": a.label, "gpus": cfg["gpu"], "env": a.env, "arg": a.arg, "drop": a.drop_arg, "log": cfg["log"]}
    try:
        t0 = time.time()
        while True:
            if srv.poll() is not None:
                raise RuntimeError(f"server exited ({srv.returncode}); see {srv_log.name}")
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{a.port}/v1/models", timeout=2)
                break
            except Exception:
                if time.time() - t0 > 900:
                    raise RuntimeError("server not ready after 900 s")
                time.sleep(2)
        res["start_s"] = round(time.time() - t0, 1)
        post(a.port, {"messages": [{"role": "user", "content": "Say hi."}], "max_tokens": 32, "temperature": 0})
        runs = []
        if a.prefill:
            doc = open(os.path.join(ROOT, "docs", "AMD_HIP.md")).read()[:15000]
            for rep in range(a.reps):
                r = post(a.port, {"messages": [{"role": "user", "content": f"Run {rep} {time.time()}. Summarize:\n\n"
                                  + doc}], "max_tokens": 1, "temperature": 0})
                t = r["timings"]
                runs.append({"prompt": "prefill", "rep": rep, "tok_s": t["prompt_per_second"], "n": t["prompt_n"],
                             "draft": None, "acc": None, "sha": "-", "head": ""})
            res["runs"] = runs
            res["prefill"] = statistics.median(x["tok_s"] for x in runs)
            res["median"] = res["prefill"]
            res["shas"] = []
            return_early = True
        else:
            return_early = False
        for rep in range(0 if return_early else a.reps):
            for name, text in PROMPTS.items():
                r = post(a.port, {"messages": [{"role": "user", "content": text}], "max_tokens": a.tokens,
                                  "temperature": 0})
                t = r["timings"]
                msg = r["choices"][0]["message"]
                content = (msg.get("reasoning_content") or "") + "\n---\n" + (msg.get("content") or "")
                runs.append({"prompt": name, "rep": rep, "tok_s": t["predicted_per_second"], "n": t["predicted_n"],
                             "draft": t.get("draft_n"), "acc": t.get("draft_n_accepted"),
                             "sha": hashlib.sha1(content.encode()).hexdigest()[:10], "head": content[:60]})
        res["runs"] = runs
        for name in ([] if return_early else PROMPTS):
            res[name] = statistics.median(x["tok_s"] for x in runs if x["prompt"] == name)
        if not return_early:
            res["median"] = statistics.median(x["tok_s"] for x in runs)
        if not return_early:
            res["shas"] = sorted({f'{x["prompt"]}:{x["sha"]}' for x in runs})
    except Exception as e:
        res["error"] = str(e)
    finally:
        os.killpg(srv.pid, signal.SIGTERM)
        try:
            srv.wait(30)
        except subprocess.TimeoutExpired:
            os.killpg(srv.pid, signal.SIGKILL)
            srv.wait()
        # the engine is the server's child; wait until it has let go of the GPUs
        for _ in range(60):
            stats = subprocess.run(["ps", "-C", "strata", "-o", "stat="], capture_output=True, text=True).stdout
            if all(s.startswith("Z") for s in stats.split()):     # zombies hold no GPU memory
                break
            time.sleep(1)
    with open(a.out, "a") as f:
        f.write(json.dumps(res) + "\n")
    print(json.dumps({k: v for k, v in res.items() if k != "runs"}))
    return 1 if "error" in res else 0


if __name__ == "__main__":
    sys.exit(main())
