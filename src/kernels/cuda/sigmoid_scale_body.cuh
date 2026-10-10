// src/kernels/cuda/sigmoid_scale_body.cuh - shared_expert.cu's sigmoid_scale_rows_vec4_kernel element: row t's float4
// i scaled by sigmoid(g[t]).  The kernel and the fused gate/scale/head launch (native_bf16.cu) run this same code.
#pragma once
#include <cuda_runtime.h>

namespace strata::kernels {
__device__ __forceinline__ void sigmoid_scale_vec4_elem(float4* __restrict__ out4, const float* __restrict__ g, int t, int i,
                                                        int n4) {
    const float gt = __fdividef(1.0f, 1.0f + __expf(-__ldg(g + t)));
    float4 v = out4[(size_t) t * n4 + i];
    v.x *= gt;
    v.y *= gt;
    v.z *= gt;
    v.w *= gt;
    out4[(size_t) t * n4 + i] = v;
}
}  // namespace strata::kernels
