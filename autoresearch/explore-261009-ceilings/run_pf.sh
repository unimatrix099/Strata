#!/usr/bin/env bash
cd /workspace/autoresearch/explore-261009-ceilings
hipcc -O2 --offload-arch=gfx1100 gemm_tflops.cpp -o gemm_tflops -L/opt/rocm/lib -lhipblas 2>&1 | grep -E "error" | head -3
./gemm_tflops 2>&1 | tee gemm_tflops.txt
cd /workspace
python3 ~/strata-tools/longctx.py --label pf-timing --out ~/strata-tools/longctx/pf-timing --tests speed --speed-sizes 8k,32k --env STRATA_PREFILL_TIMING=1 > ~/strata-tools/pf-timing.txt 2>&1
echo PFDONE
