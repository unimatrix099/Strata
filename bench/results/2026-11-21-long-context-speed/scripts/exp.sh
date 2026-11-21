#!/bin/bash
# exp.sh LABEL EXE "ENV=1 ENV2=x" "extra args" [reps] [mode] : fresh engine on :8091 with production config + changes, bench, stop
LABEL=$1; EXE=$2; ENVS=$3; XARGS=$4; REPS=${5:-2}; MODE=${6:-8000,128000,240000}
D=/workspace/ar1121; PY=/workspace/Strata/.venv/bin/python
mkdir -p $D/run
python3 - "$LABEL" "$EXE" "$ENVS" "$XARGS" <<'PY'
import json, sys
label, exe, envs, xargs = sys.argv[1:5]
c = json.load(open("/workspace/Strata/strata-swift-iq3_s-tuned.json"))
c["exe"] = exe; c["port"] = 8091; c["log"] = f"/workspace/ar1121/run/{label}-engine.txt"
for kv in envs.split():
    k, v = kv.split("=", 1)
    if v == "UNSET": c["env"].pop(k, None)
    else: c["env"][k] = v
xs = xargs.split()
i = 0
while i < len(xs):
    a = xs[i]
    if a.startswith("--drop="):
        n = a[7:]
        if n in c["args"]:
            j = c["args"].index(n); nv = int(xs[i+1]) if False else 0
            # drop flag and its value if the next item is not a flag
            if j + 1 < len(c["args"]) and not c["args"][j+1].startswith("--"): del c["args"][j:j+2]
            else: del c["args"][j]
    elif a.startswith("--") and i + 1 < len(xs) and not xs[i+1].startswith("--"):
        if a in c["args"]: c["args"][c["args"].index(a) + 1] = xs[i+1]
        else: c["args"] += [a, xs[i+1]]
        i += 1
    else:
        if a not in c["args"]: c["args"].append(a)
    i += 1
json.dump(c, open(f"/workspace/ar1121/run/{label}.json", "w"), indent=1)
PY
cd /workspace/Strata/terminal
setsid nohup $PY serve/server.py --engine strata --config $D/run/$LABEL.json --port 8091 > $D/run/$LABEL-serve.txt 2>&1 < /dev/null &
ok=0; for i in $(seq 1 100); do sleep 3; curl -s -m 2 localhost:8091/health | grep -q '"loaded": true' && { ok=1; break; }; done
if [ $ok = 0 ]; then echo "$LABEL NOT LOADED"; tail -5 $D/run/$LABEL-serve.txt; tail -5 $D/run/$LABEL-engine.txt; else
[ -n "$VALIDATE" ] && python3 $D/validate.py 8091 $LABEL 2>&1 | tail -12
python3 $D/ctx.py 8091 $LABEL "$MODE" 2>&1 | grep -E "CTX |sha|Error|error"; fi
echo "$LABEL GTT MiB: $(for c in /sys/class/drm/card[01]/device; do echo -n "$(( $(cat $c/mem_info_gtt_used) / 1048576 )) vram $(( $(cat $c/mem_info_vram_used) / 1048576 )) | "; done) $(grep -hoE "expert cache [0-9]+ slots" $D/run/$LABEL-engine.txt | tr "\n" " ")"
[ -n "$EXTRA_BENCH" ] && python3 /workspace/ar1120/bench.py 8091 $LABEL 2 all 2>&1 | grep -E "DECODE median|PREFILL median"
python3 $D/summ.py $LABEL
$D/stop.sh 8091
