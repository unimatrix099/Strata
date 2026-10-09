#!/usr/bin/env bash
# step 15: a kernel trace of the prompt path (a 4K prompt, production config) - which kernels take the time
cd /workspace
until grep -q S14DONE ~/strata-tools/loop14s.txt; do sleep 30; done
P=~/strata-tools/prof-prompt; rm -rf $P; mkdir -p $P
cat > $P/wrap.sh <<'W'
#!/usr/bin/env bash
exec /opt/rocm/bin/rocprofv3 --kernel-trace -f csv -d "$HOME/strata-tools/prof-prompt" -o run -- /workspace/engine/strata "$@"
W
chmod +x $P/wrap.sh
python3 ~/strata-tools/longctx.py --label s15-prof --out ~/strata-tools/longctx/s15-prof --tests speed --speed-sizes 4k --exe $P/wrap.sh
echo S15DONE
