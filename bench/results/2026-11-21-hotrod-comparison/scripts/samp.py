import json,sys,urllib.request,statistics
port=int(sys.argv[1]); P=["Write a Python function that merges two sorted lists, with a docstring and two tests.","Explain in two paragraphs how a refrigerator moves heat from inside to outside.","List twelve European capitals with one sentence about each."]
def run(extra):
    out=[]
    for p in P:
        b={"messages":[{"role":"user","content":p}],"max_tokens":400,**extra}
        r=json.load(urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions",json.dumps(b).encode(),{"Content-Type":"application/json"}),timeout=600))
        t=r["timings"]; out.append((t["predicted_per_second"],t.get("draft_n"),t.get("draft_n_accepted")))
    return out
cases={"greedy":{"temperature":0},"t1.0 std":{"temperature":1.0,"top_p":0.95,"top_k":20},"t1.0 no top_p/k":{"temperature":1.0},"t0.6 top_k20":{"temperature":0.6,"top_p":0.95,"top_k":20},"t1.0 +seed":{"temperature":1.0,"top_p":0.95,"top_k":20,"seed":1}}
run({"temperature":0})
for n,e in cases.items():
    o=run(e); print(f"{n:18s} tok/s {[x[0] for x in o]} accept {sum(x[2] or 0 for x in o)}/{sum(x[1] or 0 for x in o)}",flush=True)
