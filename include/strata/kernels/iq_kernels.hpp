// include/strata/kernels/iq_kernels.hpp - the i-quant formats (IQ1_M, IQ2_XXS, IQ2_XS, IQ2_S, IQ3_XXS, IQ3_S,
// IQ4_NL) and Q2_0 on the GPU for the IQ2_XS / IQ3_XXS model files, and Q4_K / Q5_K / Q5_1 / Q8_0 for Unsloth's
// UD-Q4_K_XL (gate/up Q4_K or Q5_K, down Q5_1 or Q8_0, a Q8_0 embedding).
//
// The block layouts, codebook grids and dot products are llama.cpp's (ggml-common.h, ggml-cuda/vecdotq.cuh,
// ggml-cuda/dequantize.cuh; MIT, see third_party/ggml/LICENSE and VERSION.txt), so a weight means exactly what it
// means in llama.cpp.  Activations are q8_1 (32 values, fp16 scale and fp16 sum), the llama.cpp CUDA contract.
#pragma once

#include <cstddef>
#include <cstdint>

namespace strata::kernels {

/// ggml type ids handled here.
bool iq_supported(int ggml_type) noexcept;
/// The token-embedding types iq_embed_rows and iq_dequant_f32 read: the i-quants above and BF16 (30).
bool embed_type_supported(int ggml_type) noexcept;
/// Bytes of one row of `n` values of `ggml_type` (n a multiple of the type's block).
size_t iq_row_bytes(int ggml_type, int64_t n) noexcept;

/// q8_1 blocks for `n_rows` rows of `n_cols` floats (n_cols a multiple of 32): y is n_rows * n_cols/32 blocks.
void quantize_q8_1_rows(const float* x, int64_t n_rows, int64_t n_cols, void* y, void* stream);

/// y[c][r] = W[r] . x[c] for `ncols` columns of q8_1 activations (x stride n_in/32 blocks per column).
void iq_mmvq(int ggml_type, const void* w, const void* x_q8_1, float* y, int n_in, int n_out, int ncols, void* stream);

/// Dequantize `n` contiguous values (n a multiple of 256) to fp16 / fp32.
void iq_dequant_f16(int ggml_type, const void* src, int64_t n, uint16_t* dst, void* stream);
void iq_dequant_f32(int ggml_type, const void* src, int64_t n, float* dst, void* stream);
/// Rows `tokens[0..n_tok)` (device ids) of a GGUF embedding table (`row_bytes` per row; the table may be mapped
/// host memory) dequantized to fp32, `n_embd` per row (a multiple of 256).
void iq_embed_rows(int ggml_type, const void* table, size_t row_bytes, const int32_t* tokens, int64_t n_tok,
                   int64_t n_embd, float* out, void* stream);
/// One expert's gate and up matrices (n_ff rows of n_embd each) into the interleaved fp16 layout the prompt path
/// uses: row 2r = gate row r, row 2r+1 = up row r.
void iq_dequant_gu_f16(int ggml_type, const void* gate, const void* up, int64_t n_ff, int64_t n_embd, uint16_t* dst,
                       void* stream);

/// The layout of one native expert blob: [gate rows | up rows | down rows], raw GGUF blocks.
struct NativeExpertLayout {
    int gu_type = -1, d_type = -1;
    int64_t n_embd = 0, n_ff = 0;
    size_t gu_row = 0, d_row = 0;       // bytes per row
    size_t up_off = 0, down_off = 0;    // byte offsets inside the blob
    size_t bytes = 0;                   // the whole blob
};
NativeExpertLayout native_expert_layout(int gu_type, int d_type, int64_t n_embd, int64_t n_ff);
/// Whether `native_expert_grouped` has kernels for this gate/up and down type pair at these dimensions, and the
/// prompt path's dequantizer takes both (checked for every layer at startup, before anything is allocated).
bool native_expert_supported(int gu_type, int d_type, int64_t n_embd, int64_t n_ff) noexcept;

/// Bytes of scratch `native_expert_grouped` needs for `cap_entries` entries.
size_t native_expert_scratch_bytes(int64_t cap_entries, int64_t n_ff);

/// Grouped experts in the native format: group g's blob at device address grp_ptr[g]; its entries
/// [grp_start[g], grp_start[g+1]) read token ent_tok[e]'s q8_1 activation (n_embd/32 blocks per token in x_q8_1)
/// and write row ent_dst[e] of `out` (n_embd floats).  Counts are read on the device.
/// `grid_groups` (1 .. cap_groups; 0 = cap_groups) groups run side by side, a block row each striding over the rest:
/// a call that usually has few groups or none (the verify window's PCIe share) launches less for the ones it does
/// not have.  The results do not depend on it.
void native_expert_grouped(const NativeExpertLayout& L, const unsigned long long* grp_ptr, const int32_t* grp_start,
                           const int32_t* n_groups, const int32_t* ent_dst, const int32_t* ent_tok, int64_t cap_groups,
                           int64_t cap_entries, const void* x_q8_1, void* scratch, float* out, void* stream,
                           int64_t grid_groups = 0);
/// true: `native_expert_grouped`'s launches before the group stride (STRATA_GROUPED_V1=1 at startup) - a block row
/// per possible group, SwiGLU and the q8_1 quantization as two kernels over all cap_entries.  Bitwise the same results
/// (native_grouped_parity checks it); kept for A/B timing.  Set before graph capture; captured graphs keep theirs.
void native_grouped_set_v1(bool v1);

/// The bench only: the AMD kernel layout (STRATA_EXP_MODE values; -1 = the environment's) and the phase
/// (0 all, 1 gate/up + SwiGLU + quantize, 2 down).
void native_expert_set_mode(int mode, int phase);
/// `iq_mmvq` and `native_expert_grouped` decode each weight part once and apply it to every column / entry;
/// true selects the older kernels that decode it again per column (STRATA_OLD_IQ_MMVQ=1 at startup).  Both give
/// bitwise the same results.  Set before graph capture; captured graphs keep the kernels they captured.
void iq_set_old_kernels(bool old);
bool iq_old_kernels();


/// The persistent FFN half, part 1: verify.cpp's post() for one layer group - wait A, the plan's copy from mapped
/// memory, the VRAM experts (gate/up, SwiGLU + q8_1, down), wait B, the PCIe share (fetch, rebase, its pass), wait C
/// and the combine (native_moe_combine_gather_multi) - as ONE launch of `blocks` resident blocks, bit for bit the
/// kernels' values (the same kernel bodies over virtual grids, a grid barrier between the steps).  The plan layout is
/// the Verifier's: counts | start | dst | tok | pad | ptr | ptr2 | start2 (`capx` entries, ptr at int32 `ptr_off`).
/// HIP only; false (nothing launched) when the grouped launcher would take a path this does not cover.
struct NativeExpertPostArgs {
    const uint32_t* flag_a = nullptr; const uint32_t* flag_b = nullptr; const uint32_t* flag_c = nullptr;
    uint32_t ring = 0;
    const int32_t* m_plan = nullptr;     // the pool's plan, mapped host memory
    int32_t* plan = nullptr;             // its device copy
    int plan_i32 = 0;
    long long capx = 0, ptr_off = 0;
    long long cap = 0;                   // the call's entries (n * k), as native_expert_grouped's cap
    const void* xq = nullptr;            // the rows' q8_1 image
    void* scratch = nullptr;             // native_expert_scratch_bytes(cap, n_ff)
    float* hit_out = nullptr;            // [cap][n_embd]: the GPU's expert rows
    uint8_t* stage = nullptr; long long blob_bytes = 0;   // the PCIe share's staging
    const float* cpu_rows = nullptr;     // the CPU's rows (mapped)
    const float* weights = nullptr; const float* shared = nullptr; float* output = nullptr;
    int k = 10, n_tok = 1;
    unsigned* bar = nullptr;             // 2 words of device memory, zeroed once (the barrier resets itself)
    int blocks = 0;
    int gu_gx = 0, sw_gx = 0, d_gx = 0;  // set by the launcher
    unsigned long long* stamps = nullptr; // (diagnostic) block 0's clock after each barrier, 16 slots
    int part = 0;                        // 0 all of post(); 1 the head (wait A + the plan's copy); 2 the tail (wait B,
                                         // the PCIe share, wait C, the combine): the VRAM experts' kernels between
};
bool native_expert_post_persistent(const NativeExpertPostArgs& args, const NativeExpertLayout& L, void* stream);
/// The most blocks of the persistent kernel that are resident at once on this device (a larger grid would deadlock).
int native_expert_post_max_blocks();

}  // namespace strata::kernels
