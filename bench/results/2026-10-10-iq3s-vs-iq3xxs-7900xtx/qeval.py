#!/usr/bin/env python3
"""A small known-answer quality check for one engine configuration: 32 multi-step math word problems (templates
with random numbers; the exact answer computed here) and 8 Python tasks checked by running unit tests.  Greedy,
thinking on (the model's normal mode), one server per run.

  qeval.py --label iq3xxs --config /workspace/strata-iq3_xxs.json
Prints one JSON line per item and a summary; writes <out>/<label>.jsonl.
"""
import argparse, json, os, random, re, signal, subprocess, sys, tempfile, time, urllib.request

ROOT = "/workspace"
PORT = 8093


def post(body, timeout=1800):
    req = urllib.request.Request(f"http://127.0.0.1:{PORT}/v1/chat/completions", json.dumps(body).encode(),
                                 {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def math_items(seed=20261010):
    rng = random.Random(seed)
    items = []
    for i in range(4):
        a, b, c = rng.randint(12, 48), rng.randint(3, 9), rng.randint(5, 19)
        items.append((f"A bakery makes {a} trays of rolls with {b} rolls per tray. It sells all but {c} rolls, and then "
                      f"bakes 2 more trays. How many rolls does it have now?", a * b - (a * b - c) + 2 * b))
        p, q, r = rng.randint(120, 480), rng.randint(15, 45), rng.randint(2, 6)
        items.append((f"A train travels {p} km at {q * 2} km/h, then waits {r * 10} minutes, then travels another {p // 2} km "
                      f"at {q * 3} km/h. How many minutes does the whole trip take? Give the exact number of minutes as a "
                      f"fraction or decimal if needed.",
                      p / (q * 2) * 60 + r * 10 + (p // 2) / (q * 3) * 60))
        n, k = rng.randint(30, 90), rng.randint(3, 7)
        items.append((f"What is the sum of all integers from 1 to {n} that are divisible by {k}?",
                      sum(x for x in range(1, n + 1) if x % k == 0)))
        x, y = rng.randint(200, 900), rng.randint(5, 25)
        items.append((f"A jacket costs {x} dollars. It is discounted by {y}%, and then a 10% tax is added to the "
                      f"discounted price. What is the final price in dollars? Round to the nearest cent.",
                      round(x * (1 - y / 100) * 1.1, 2)))
        m = rng.randint(4, 9)
        items.append((f"In how many ways can you choose 3 people from a group of {m + 6}?",
                      (m + 6) * (m + 5) * (m + 4) // 6))
        u, v = rng.randint(11, 39), rng.randint(11, 39)
        items.append((f"Compute {u} * {v} - {u + v} * 3 + {u} mod {v % 9 + 2}. ('mod' binds like multiplication.)",
                      u * v - (u + v) * 3 + u % (v % 9 + 2)))
        s = rng.randint(3, 9)
        items.append((f"A rectangle's length is {s} cm more than twice its width, and its perimeter is {2 * (3 * 7 + s)} cm. "
                      f"What is its area in square cm?", 7 * (2 * 7 + s)))
        t = rng.randint(2, 5)
        items.append((f"Alice is {t} times as old as Bob. In 12 years she will be twice as old as Bob. How old is Bob now? "
                      f"Give the exact value.", 12 / (t - 2) if t != 2 else None))
    # the train items' answers are fractional minutes: a model may round them to 0.1
    return [(q, a, 0.1 if q.startswith("A train") else 0.011) for q, a in items if a is not None][:32]


CODE = [
    ("Write a Python function `is_palindrome(s)` that returns True if s reads the same backwards ignoring case and all "
     "non-alphanumeric characters.",
     "assert is_palindrome('A man, a plan, a canal: Panama')\nassert not is_palindrome('race a car')\nassert is_palindrome('')"),
    ("Write a Python function `merge_intervals(iv)` that merges overlapping intervals given as a list of [start, end] "
     "lists and returns them sorted by start.",
     "assert merge_intervals([[1,3],[2,6],[8,10],[15,18]]) == [[1,6],[8,10],[15,18]]\nassert merge_intervals([[1,4],[4,5]]) == [[1,5]]\nassert merge_intervals([]) == []"),
    ("Write a Python function `roman_to_int(s)` that converts a Roman numeral string to an integer.",
     "assert roman_to_int('MCMXCIV') == 1994\nassert roman_to_int('LVIII') == 58\nassert roman_to_int('IV') == 4"),
    ("Write a Python function `top_k_frequent(nums, k)` returning the k most frequent numbers, most frequent first; ties "
     "broken by the smaller number first.",
     "assert top_k_frequent([1,1,1,2,2,3], 2) == [1,2]\nassert top_k_frequent([4,4,5,5,6], 2) == [4,5]"),
    ("Write a Python function `lcs_length(a, b)` returning the length of the longest common subsequence of two strings.",
     "assert lcs_length('abcde', 'ace') == 3\nassert lcs_length('abc', 'def') == 0\nassert lcs_length('', 'x') == 0"),
    ("Write a Python function `valid_parentheses(s)` that returns True if every bracket in s ('()[]{}') is closed in the "
     "right order.",
     "assert valid_parentheses('()[]{}')\nassert not valid_parentheses('(]')\nassert valid_parentheses('{[]}')\nassert not valid_parentheses('((')"),
    ("Write a Python function `spiral(matrix)` returning the elements of a 2D list in clockwise spiral order.",
     "assert spiral([[1,2,3],[4,5,6],[7,8,9]]) == [1,2,3,6,9,8,7,4,5]\nassert spiral([[1,2,3,4],[5,6,7,8],[9,10,11,12]]) == [1,2,3,4,8,12,11,10,9,5,6,7]\nassert spiral([]) == []"),
    ("Write a Python function `num_islands(grid)` counting groups of '1' cells connected horizontally or vertically in a "
     "2D list of '0'/'1' strings.",
     "assert num_islands([['1','1','0'],['0','1','0'],['0','0','1']]) == 2\nassert num_islands([]) == 0"),
]


def hard_math_items(seed=7):
    """Exact multi-step computations a model must carry out without slips."""
    from math import comb
    rng = random.Random(seed)
    def primes_below(n):
        sieve = [True] * n
        sieve[0:2] = [False, False]
        for i in range(2, int(n ** 0.5) + 1):
            if sieve[i]:
                sieve[i * i::i] = [False] * len(sieve[i * i::i])
        return [i for i, v in enumerate(sieve) if v]
    def ndiv(n):
        c, d = 0, 1
        while d * d <= n:
            if n % d == 0: c += 1 if d * d == n else 2
            d += 1
        return c
    def derange(n):
        a, b = 1, 0
        for k in range(2, n + 1): a, b = b, (k - 1) * (a + b)
        return b if n > 0 else 1
    def fib(n):
        a, b = 0, 1
        for _ in range(n): a, b = b, a + b
        return a
    items = []
    for _ in range(4):
        a, b, m = rng.randint(3, 19), rng.randint(40, 120), rng.randint(50, 997)
        items.append((f"Compute {a}^{b} mod {m}.", pow(a, b, m), 0.011))
        n = rng.randint(300, 1500)
        items.append((f"What is the sum of all prime numbers less than {n}?", sum(primes_below(n)), 0.011))
        n = rng.choice([2 ** rng.randint(2, 6) * 3 ** rng.randint(1, 4) * rng.choice([5, 7, 11, 13]) * rng.choice([1, 17, 19])])
        items.append((f"How many positive divisors does {n} have?", ndiv(n), 0.011))
        k = rng.randint(6, 10)
        items.append((f"How many permutations of {k} distinct items leave no item in its original position?", derange(k), 0.011))
        w, h = rng.randint(5, 8), rng.randint(5, 8)
        i, j = rng.randint(1, w - 1), rng.randint(1, h - 1)
        total = comb(w + h, w) - comb(i + j, i) * comb(w + h - i - j, w - i)
        items.append((f"On a grid, how many shortest paths go from (0,0) to ({w},{h}) moving only one step right (+x) or "
                      f"up (+y) at a time, if the point ({i},{j}) is blocked and may not be visited?", total, 0.011))
        t = rng.randint(14, 20)
        p = sum(1 for x in range(1, 7) for y in range(1, 7) for z in range(1, 7) if x + y + z >= t) / 216
        items.append((f"Three fair six-sided dice are rolled. What is the probability that their sum is at least {t}? "
                      f"Give it as a decimal rounded to 4 places.", round(p, 4), 0.00011))
        n, q = rng.randint(1000, 4000), rng.choice([5, 7, 9])
        items.append((f"How many integers from 1 to {n} inclusive have a digit sum divisible by {q}?",
                      sum(1 for x in range(1, n + 1) if sum(map(int, str(x))) % q == 0), 0.011))
        n, m = rng.randint(50, 90), rng.randint(97, 1009)
        items.append((f"Let F(0)=0, F(1)=1, F(n)=F(n-1)+F(n-2). What is F({n}) mod {m}?", fib(n) % m, 0.011))
    return items


HARD_CODE = [
    ("Write a Python function `edit_distance(a, b)` returning the Levenshtein distance (insert, delete, substitute, each "
     "cost 1).",
     "assert edit_distance('kitten','sitting')==3\nassert edit_distance('','abc')==3\nassert edit_distance('intention','execution')==5"),
    ("Write a Python function `is_match(s, p)` implementing regular-expression matching over the whole string where '.' "
     "matches any single character and '*' matches zero or more of the preceding element.",
     "assert not is_match('aa','a')\nassert is_match('aa','a*')\nassert is_match('ab','.*')\nassert is_match('aab','c*a*b')\nassert not is_match('mississippi','mis*is*p*.')"),
    ("Write a Python function `lis_length(nums)` returning the length of the longest strictly increasing subsequence in "
     "O(n log n).",
     "assert lis_length([10,9,2,5,3,7,101,18])==4\nassert lis_length([0,1,0,3,2,3])==4\nassert lis_length([7,7,7])==1\nassert lis_length([])==0\nimport random\nrandom.seed(1)\nxs=[random.randint(0,10**6) for _ in range(200000)]\nlis_length(xs)"),
    ("Write a Python function `topo_order(n, edges)` that returns a list ordering nodes 0..n-1 so every edge (u, v) has u "
     "before v, choosing the smallest available node first at each step; return None if there is a cycle.",
     "assert topo_order(4,[(0,1),(1,2),(0,3)])==[0,1,2,3]\nassert topo_order(3,[(2,1),(1,0)])==[2,1,0]\nassert topo_order(2,[(0,1),(1,0)]) is None\nassert topo_order(3,[])==[0,1,2]"),
    ("Write a Python function `n_queens(n)` returning the number of ways to place n non-attacking queens on an n x n "
     "board.",
     "assert n_queens(1)==1\nassert n_queens(4)==2\nassert n_queens(6)==4\nassert n_queens(8)==92"),
    ("Write a Python function `dijkstra(n, edges, src)` where edges is a list of (u, v, w) undirected weighted edges; "
     "return a list of shortest distances from src to every node, using float('inf') for unreachable nodes.",
     "assert dijkstra(4,[(0,1,1),(1,2,2),(0,2,5)],0)==[0,1,3,float('inf')]\nassert dijkstra(1,[],0)==[0]"),
    ("Write a Python class `LRUCache` with __init__(capacity), get(key) returning -1 if absent, and put(key, value), "
     "evicting the least recently used key when over capacity; get and put must be O(1).",
     "c=LRUCache(2)\nc.put(1,1);c.put(2,2)\nassert c.get(1)==1\nc.put(3,3)\nassert c.get(2)==-1\nc.put(4,4)\nassert c.get(1)==-1\nassert c.get(3)==3\nassert c.get(4)==4"),
    ("Write a Python function `max_non_overlapping(intervals)` returning the maximum number of pairwise non-overlapping "
     "intervals from a list of (start, end) pairs, where intervals that only touch at an endpoint do not overlap.",
     "assert max_non_overlapping([(1,2),(2,3),(3,4),(1,3)])==3\nassert max_non_overlapping([(1,2),(1,2),(1,2)])==1\nassert max_non_overlapping([])==0"),
]


def last_number(text):
    nums = re.findall(r"-?\d[\d,]*\.?\d*(?:/\d+)?", text.replace("$", ""))
    if not nums:
        return None
    s = nums[-1].replace(",", "").rstrip(".")
    try:
        if "/" in s:
            p, q = s.split("/")
            return float(p) / float(q)
        return float(s)
    except ValueError:
        return None


def extract_code(text):
    blocks = re.findall(r"```(?:python)?\n(.*?)```", text, re.S)
    return blocks[-1] if blocks else text


def run_tests(code, tests):
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
        f.write(code + "\n\n" + tests + "\nprint('OK')\n")
        path = f.name
    try:
        r = subprocess.run([sys.executable, path], capture_output=True, text=True, timeout=20)
        return r.returncode == 0 and "OK" in r.stdout
    except subprocess.TimeoutExpired:
        return False
    finally:
        os.unlink(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--config", required=True)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "data", "qeval"))
    ap.add_argument("--max-tokens", type=int, default=12000)
    ap.add_argument("--set", default="easy", choices=["easy", "hard", "both"])
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    cfg = json.load(open(a.config))
    logs = os.path.join(os.path.dirname(__file__), "data", "logs")
    os.makedirs(logs, exist_ok=True)
    stamp = time.strftime("%H%M%S")
    cfg["log"] = os.path.join(logs, f"qeval-{a.label}-{stamp}.engine.txt")
    cfg_path = os.path.join(logs, f"qeval-{a.label}-{stamp}.config.json")
    json.dump(cfg, open(cfg_path, "w"), indent=1)
    srv = subprocess.Popen([os.path.join(ROOT, "." + "venv", "bin", "python"), os.path.join(ROOT, "serve", "server.py"),
                            "--engine", "strata", "--config", cfg_path, "--port", str(PORT)],
                           stdout=open(os.path.join(logs, f"qeval-{a.label}-{stamp}.server.txt"), "w"),
                           stderr=subprocess.STDOUT, start_new_session=True)
    try:
        t0 = time.time()
        while True:
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{PORT}/v1/models", timeout=5)
                break
            except Exception:
                if srv.poll() is not None or time.time() - t0 > 900:
                    print("server did not start"); return 1
                time.sleep(5)
        res = []
        outf = open(os.path.join(a.out, f"{a.label}-{a.set}.jsonl"), "w")
        items = []
        if a.set in ("easy", "both"):
            items += [("math", q, (ans, tol)) for q, ans, tol in math_items()] + [("code", q, t) for q, t in CODE]
        if a.set in ("hard", "both"):
            items += [("math", q, (ans, tol)) for q, ans, tol in hard_math_items()] + [("code", q, t) for q, t in HARD_CODE]
        for i, (kind, q, ref) in enumerate(items):
            prompt = q + ("\nEnd your answer with a last line of the form 'ANSWER: <number>'." if kind == "math"
                          else "\nGive the complete function in one ```python code block.")
            t1 = time.time()
            r = post({"messages": [{"role": "user", "content": prompt}], "temperature": 0, "max_tokens": a.max_tokens})
            dt = time.time() - t1
            msg = r["choices"][0]["message"]
            text = msg.get("content") or ""
            fin = r["choices"][0].get("finish_reason")
            if kind == "math":
                m = re.findall(r"ANSWER:\s*(.*)", text)
                got = last_number(m[-1] if m else text)
                ref, tol = ref
                ok = got is not None and abs(got - float(ref)) <= tol
                key = None if got is None else round(got, 2)
            else:
                code = extract_code(text)
                ok = run_tests(code, ref)
                key = ok
            row = {"i": i, "kind": kind, "ok": ok, "key": key, "ref": ref if kind == "math" else None,
                   "finish": fin, "tokens": r.get("usage", {}).get("completion_tokens"), "s": round(dt, 1),
                   "answer_tail": text[-160:]}
            res.append(row)
            outf.write(json.dumps(row) + "\n"); outf.flush()
            print(json.dumps({k: row[k] for k in ("i", "kind", "ok", "key", "ref", "finish", "tokens", "s")}), flush=True)
        mt = [r for r in res if r["kind"] == "math"]; ct = [r for r in res if r["kind"] == "code"]
        print(json.dumps({"label": a.label, "set": a.set, "math": f"{sum(r['ok'] for r in mt)}/{len(mt)}",
                          "code": f"{sum(r['ok'] for r in ct)}/{len(ct)}",
                          "tokens": sum(r["tokens"] or 0 for r in res), "s": round(sum(r["s"] for r in res))}))
    finally:
        os.killpg(srv.pid, signal.SIGTERM)
        try:
            srv.wait(60)
        except subprocess.TimeoutExpired:
            os.killpg(srv.pid, signal.SIGKILL)
    return 0


if __name__ == "__main__":
    sys.exit(main())
