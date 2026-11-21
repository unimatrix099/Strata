import json,sys,time,urllib.request
port,label=sys.argv[1],sys.argv[2]
for n in (4000,4000,4000,16000,16000,32000):
    txt="".join(f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1,n//17))
    b={"messages":[{"role":"user","content":f"[pf {time.time()}]\n"+txt+"Summarize in one sentence."}],"max_tokens":8,"temperature":0,"cache_prompt":False}
    d=json.load(urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions",json.dumps(b).encode(),{"Content-Type":"application/json"}),timeout=3600))
    t=d["timings"]; print(label,n,"prompt_n",t["prompt_n"],"%.0f tok/s"%t["prompt_per_second"],"cache_n",t.get("cache_n"),flush=True)
