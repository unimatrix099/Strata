// src/kernels/cuda/vblock.cuh - a block's coordinates as a value.  A kernel body written against VBlock runs as
// its own launch (hw_vblock(): the hardware's coordinates) or as one "virtual block" of a persistent kernel that
// loops over a grid of them (the persistent FFN half, native_expert_post_persistent in iq_kernels.cu): the same code
// and the same sums either way.
#pragma once
#include <cuda_runtime.h>

namespace strata::kernels {
struct VBlock { int bx, by, gx, gy; };
__device__ __forceinline__ VBlock hw_vblock() {
    return VBlock{(int) blockIdx.x, (int) blockIdx.y, (int) gridDim.x, (int) gridDim.y};
}
}  // namespace strata::kernels
