import json,sys,time,urllib.request
port=sys.argv[1]
for tgt in (128000,245000):
    n=int(tgt/1.13)
    txt="".join(f"record{i:06d}: alpha beta gamma delta epsilon zeta eta theta.\n" for i in range(1,n//17))
    b={"messages":[{"role":"user","content":f"[ld {time.time()}]\n"+txt+"\nNow ignore the records above and write a 400-word story about a lighthouse keeper."}],"max_tokens":500,"temperature":0,"cache_prompt":False}
    t0=time.time()
    d=json.load(urllib.request.urlopen(urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions",json.dumps(b).encode(),{"Content-Type":"application/json"}),timeout=1800))
    t=d["timings"]; print(f"ctx~{t['prompt_n']} prompt {t['prompt_per_second']:.0f} tok/s ({t['prompt_ms']/1000:.0f} s) | decode {t['predicted_n']} tok at {t['predicted_per_second']:.1f} tok/s | drafts {t.get('draft_n_accepted')}/{t.get('draft_n')} | wall {time.time()-t0:.0f}s",flush=True)
