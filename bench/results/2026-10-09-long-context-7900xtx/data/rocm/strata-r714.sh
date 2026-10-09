#!/bin/sh
# the engine built against ROCm 7.14.1, with its libraries first and its own compiler cache (a cache filled by another
# ROCm breaks the start: docs/DOCKER_ROCM.md)
export LD_LIBRARY_PATH=/home/node/rocm-7.14.1/opt/rocm/core-7.14/lib:$LD_LIBRARY_PATH
export ROCM_PATH=/home/node/rocm-7.14.1/opt/rocm/core-7.14 HIP_PATH=/home/node/rocm-7.14.1/opt/rocm/core-7.14 HIP_DEVICE_LIB_PATH=/home/node/rocm-7.14.1/opt/rocm/core-7.14/lib/llvm/amdgcn/bitcode
export AMD_COMGR_CACHE=0
export STRATA_HIPBLASLT_TUNING=/workspace/tools/hip/gfx1100-hipblaslt-100401.txt
exec /workspace/build-hip-r714/strata "$@"
