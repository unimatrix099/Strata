// src/kernels/cuda/moe_combine_gather.cuh - native_moe.cu's combine_gather as a body (VBlock), so the persistent
// FFN half (iq_kernels.cu) runs the same code as the kernel: the same products, the same FMA order.
#pragma once
#include "vblock.cuh"
#include <cstdint>

namespace strata::kernels {
// combine, reading each part where it was computed: the GPU's rows (hit, +0.0 first as moe_hit_add's add onto the
// zeroed row did) or the CPU's (mapped host rows) - the same products and FMA order as combine
__device__ __forceinline__ void combine_gather_body(const VBlock vb, const float* cpu, const float* __restrict__ hit, const int32_t* __restrict__ hit_rows,
                               const int32_t* __restrict__ hit_count, const float* __restrict__ weights,
                               const float* __restrict__ shared, float* __restrict__ output, int64_t n_embd, int k) {
    const int64_t tk = vb.by;
    __shared__ unsigned gpu_rows;   // bit e: row tk * k + e is the GPU's
    if (threadIdx.x == 0) {
        unsigned m = 0;
        const int c = *hit_count;
        for (int i = 0; i < c; ++i) {
            const int64_t r = hit_rows[i] - tk * k;
            if (r >= 0 && r < k) m |= 1u << r;
        }
        gpu_rows = m;
    }
    __syncthreads();
    weights += tk * k; if (shared) shared += tk * n_embd; output += tk * n_embd;
    const int64_t col = int64_t(vb.bx) * blockDim.x + threadIdx.x;
    if (col >= n_embd) return;
    const unsigned m = gpu_rows;
    const int64_t base = tk * k * n_embd + col;
    float p = (m & 1u) ? 0.0f + hit[base] : cpu[base];
    float sum = p * weights[0];
    for (int expert = 1; expert < k; ++expert) {
        const int64_t i = base + int64_t(expert) * n_embd;
        p = (m >> expert) & 1u ? 0.0f + hit[i] : cpu[i];
        sum = fmaf(p, weights[expert], sum);   // the contract spelled out, as combine_k10_vec4 does
    }
    if (shared) sum += shared[col];
    output[col] = sum;
}
}  // namespace strata::kernels
