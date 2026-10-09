#!/bin/sh
# the engine built against ROCm 10.1, with ROCm 10.1's libraries first (the server sets its own LD_LIBRARY_PATH)
export LD_LIBRARY_PATH=/home/node/rocm-10.1/opt/rocm/core-10.1/lib:$LD_LIBRARY_PATH
export ROCM_PATH=/home/node/rocm-10.1/opt/rocm/core-10.1 HIP_PATH=/home/node/rocm-10.1/opt/rocm/core-10.1 HIP_DEVICE_LIB_PATH=/home/node/rocm-10.1/opt/rocm/core-10.1/lib/llvm/amdgcn/bitcode
export STRATA_HIPBLASLT_TUNING=/workspace/tools/hip/gfx1100-hipblaslt-100401.txt
exec /workspace/build-hip-r10/strata "$@"
