#pragma once
#include <cstdint>

namespace strata::kernels {
// Configure before constructing/capturing sessions. Existing captures keep their
// selected kernels; changing this flag does not rewrite an existing graph.
void native_moe_combine_set_enabled(bool enabled);
bool native_moe_combine_enabled();

// Pinned CUDA weighted-reduction contract for one token, k in [1, 15].
// The backend fuses k=2..15; k=1 is its ordinary multiply/contiguous/add path.
// parts: k contiguous F32 rows of n_embd; weights: k F32 values. The first
// product rounds to F32, following products accumulate with FMA in expert order,
// and optional shared is added once afterward. Shared is not router weighted.
// Requires a nonnull ordered stream and disjoint output. No allocation or sync.
void native_moe_combine(const float* parts, const float* weights, const float* shared,
                        float* output, int64_t n_embd, int64_t k, void* stream);
/// n_tok rows (parts [n,k,N], weights [n,k], shared/output [n,N]) in one launch, each as the single call.
void native_moe_combine_multi(const float* parts, const float* weights, const float* shared, float* output,
                              int64_t n_embd, int64_t k, int n_tok, void* stream);
/// native_moe_combine_multi over parts it gathers itself (the verify window's doorbell path), bit for bit the
/// copy_rows_from_mapped + moe_hit_add + native_moe_combine_multi sequence in one launch: part row r (window-relative,
/// [n_tok, k] rows of n_embd) is 0.0f + hit[r] when r is one of hit_rows[0, *hit_count) (the GPU computed it), else
/// cpu_rows[r] (the CPU's, mapped host memory).
void native_moe_combine_gather_multi(const float* cpu_rows, const float* hit, const int32_t* hit_rows,
                                     const int32_t* hit_count, const float* weights, const float* shared,
                                     float* output, int64_t n_embd, int64_t k, int n_tok, void* stream);
}
