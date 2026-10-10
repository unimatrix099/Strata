// src/kernels/ffn_half_bench.cpp - one decode window's "FFN half" of a layer (verify.cpp record_window, after the
// hyper-connection read: router GEMV, top-10, doorbell, shared expert, the plan, the VRAM experts, the PCIe pass,
// the combine) on real GGUF weights, as the production HIP window runs it: 23 kernels per layer in a graph.
//
//     build/ffn_half_bench <shard1.gguf> [shard2.gguf] [layers=1,2,..,8] [rows=2] [iters=200]
//
// The CPU's part is played up front: every routed expert is put in VRAM (98-99% are, in production), the plan is
// written into mapped memory the way the pool writes it (ExpertSource, distinct experts in routing order, their
// entries ascending), and the three host flags are raised before the graph runs, so the spin waits pass at once.
// What is timed is the GPU's own chain - what a persistent kernel for the FFN half would replace.  Several layers
// with their own weights run in one graph so the weights do not stay in the 96 MB cache.
#include "strata/artifact/gguf_reader.hpp"
#include "strata/kernels/bf16_gemv.hpp"
#include "strata/kernels/cpu/native_expert.hpp"
#include "strata/kernels/elementwise.hpp"
#include "strata/kernels/iq_kernels.hpp"
#include "strata/kernels/native_moe.hpp"
#include "strata/kernels/native_router.hpp"
#include "strata/kernels/shared_expert.hpp"
#include "strata/kernels/verify_kernels.hpp"

#include "ggml.h"

#include <cuda_runtime.h>

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace cpu = strata::kernels::cpu;
namespace K = strata::kernels;

namespace {

#define CK(x) do { cudaError_t e_ = (x); if (e_ != cudaSuccess) { std::fprintf(stderr, "%s: %s\n", #x, cudaGetErrorString(e_)); std::exit(1); } } while (0)

constexpr int64_t N = 2560, NE = 512, KR = 10, FF = 640;

struct Shards {
    std::vector<std::unique_ptr<strata::GgufFile>> f;
    std::pair<const strata::GgufFile*, const strata::TensorInfo*> find(const std::string& name) const {
        for (const auto& g : f)
            if (const auto* t = g->find(name)) return {g.get(), t};
        return {nullptr, nullptr};
    }
    const uint8_t* data(const std::pair<const strata::GgufFile*, const strata::TensorInfo*>& t) const {
        return t.first->tensor_data(*t.second);
    }
};

size_t tensor_bytes(const strata::TensorInfo& t) {
    size_t rows = 1;
    for (size_t i = 1; i < t.shape.size(); ++i) rows *= (size_t) t.shape[i];
    return ggml_row_size((ggml_type) t.type, (int64_t) t.shape[0]) * rows;
}

void* upload(const void* src, size_t bytes) {
    void* d = nullptr;
    CK(cudaMalloc(&d, bytes));
    CK(cudaMemcpy(d, src, bytes, cudaMemcpyHostToDevice));
    return d;
}

template<typename T> T* mapped(size_t count, T** dev) {
    T* h = nullptr;
    CK(cudaHostAlloc((void**) &h, count * sizeof(T), cudaHostAllocMapped));
    std::memset(h, 0, count * sizeof(T));
    CK(cudaHostGetDevicePointer((void**) dev, h, 0));
    return h;
}

// one layer's weights, inputs and plan
struct Layer {
    int l = 0;
    uint16_t* router = nullptr;     // bf16 [NE][N]
    uint16_t* sgi = nullptr;        // bf16 [N]: the shared expert's gate row
    K::NativeSharedWeights sh;
    K::NativeExpertLayout L{};
    size_t blob = 0;
    float* x = nullptr;             // [n][N] the FFN input (xm)
    void* xq = nullptr;             // its q8_1 image (QFUSE: written by the HC read before this chain)
    int32_t* h_plan = nullptr;      // mapped plan (the pool's) and its device alias
    int32_t* m_plan = nullptr;
    int32_t* plan = nullptr;        // device copy (copy_i32_from_mapped)
    uint8_t* blobs = nullptr;       // the routed experts, VRAM
    size_t router_b = 0, sgi_b = 0, sg_b = 0, su_b = 0, sd_b = 0, down_off = 0;
    int groups = 0;
    float* logits = nullptr;        // the router's real logits for x (the floor's top-10 reads them)
};


// ---- the floor: the FFN half's memory traffic and host hand-offs as phases of ONE persistent kernel
// (no expert / projection arithmetic): each phase reads the bytes its kernels read (the weights, the activations), the
// serial steps run for real on block 0 (the top-10 over the real logits, the doorbell's mapped writes and fences, the
// spin waits on the mapped flags, the plan's copy from mapped memory), and a grid barrier separates the phases.
// What it takes is the least a persistent FFN half could take on this card.
struct Region { const uint4* p; long long n, pre; };
enum : int { OP_NONE = 0, OP_TOP10 = 1, OP_DOORBELL = 2, OP_WAIT_PLAN = 3, OP_WAIT2 = 4 };
struct Phase { int op, r0, nr; long long total; int li; };
struct LayerDev {
    const float* logits; int32_t* ids; float* w;           // top-10
    const float* x; float* m_x; int32_t* m_ids; float* m_w; uint32_t* m_seq;   // doorbell
    const uint32_t* flagA; const uint32_t* flagB; const uint32_t* flagC;
    const int32_t* m_plan; int32_t* plan; int plan_i32;
};

__device__ __forceinline__ float fl_warp_sum(float v) {
    for (int m = 16; m; m >>= 1) v += __shfl_xor(v, m, 32);
    return v;
}
__device__ __forceinline__ float fl_warp_max(float v) {
    for (int m = 16; m; m >>= 1) v = fmaxf(v, __shfl_xor(v, m, 32));
    return v;
}
// native_router.cu's route_multi for one row (one wave), the same arithmetic
__device__ void fl_top10(const float* logits, int32_t* ids, float* weights, int lane) {
    float values[16];
    for (int i = 0; i < 16; ++i) values[i] = logits[lane + i * 32];
    float maximum = -INFINITY;
    for (int i = 0; i < 16; ++i) maximum = fmaxf(maximum, values[i]);
    maximum = fl_warp_max(maximum);
    float sum = 0.0f;
    for (int i = 0; i < 16; ++i) { values[i] = expf(values[i] - maximum); sum += values[i]; }
    const float reciprocal = 1.0f / fl_warp_sum(sum);
    for (int i = 0; i < 16; ++i) { values[i] *= reciprocal; if (__isnanf(values[i])) values[i] = -FLT_MAX; }
    float selected = 0.0f, selected_sum = 0.0f;
    for (int rank = 0; rank < 10; ++rank) {
        float best = values[0];
        int expert = lane;
        for (int i = 1; i < 16; ++i) if (values[i] > best) { best = values[i]; expert = lane + i * 32; }
        for (int m = 16; m; m >>= 1) {
            const float other = __shfl_xor(best, m, 32);
            const int other_id = __shfl_xor(expert, m, 32);
            if (other > best || (other == best && other_id < expert)) { best = other; expert = other_id; }
        }
        if ((expert & 31) == lane) { values[expert / 32] = -INFINITY; ids[rank] = expert; selected_sum += best; }
        if (rank == lane) selected = best;
    }
    selected_sum = fmaxf(fl_warp_sum(selected_sum), 6.103515625e-5f);
    if (lane < 10) weights[lane] = selected / selected_sum;
}
__device__ __forceinline__ void fl_spin(const uint32_t* flag, uint32_t v) {
    while (*(const volatile uint32_t*) flag < v) __builtin_amdgcn_s_sleep(1);
    __threadfence_system();
}
__device__ void fl_op(int op, const LayerDev& L, int n_rows, long long nx, int nk) {
    const int t = threadIdx.x;
    if (op == OP_TOP10) {
        if ((t >> 5) < n_rows) fl_top10(L.logits + (size_t) (t >> 5) * 512, L.ids + (t >> 5) * 10, L.w + (t >> 5) * 10, t & 31);
    } else if (op == OP_DOORBELL) {   // doorbell_publish_kernel's work (the same mapped writes, fences and ring)
        for (long long i = t; i < nx; i += blockDim.x) L.m_x[i] = L.x[i];
        for (int i = t; i < nk; i += blockDim.x) { L.m_ids[i] = L.ids[i]; L.m_w[i] = L.w[i]; }
        __threadfence_system();
        __syncthreads();
        if (t == 0) { volatile uint32_t* q = L.m_seq; *q = *q + 1; __threadfence_system(); }
    } else if (op == OP_WAIT_PLAN) {
        if (t == 0) fl_spin(L.flagA, 1);
        __syncthreads();
        for (int i = t; i < L.plan_i32; i += blockDim.x) L.plan[i] = ((const volatile int32_t*) L.m_plan)[i];
    } else if (op == OP_WAIT2) {
        if (t == 0) { fl_spin(L.flagB, 1); fl_spin(L.flagC, 1); }
    }
    __syncthreads();
}
// the region table goes to LDS first (a phase has at most kMaxRegions), so finding an element's region costs a few
// LDS reads, not dependent global loads; four loads are in flight per thread before any is used
constexpr int kMaxRegions = 64;
__device__ __forceinline__ unsigned fl_read(const Region* R, int nr, long long total, long long gtid, long long stride) {
    __shared__ long long s_pre[kMaxRegions];
    __shared__ const uint4* s_p[kMaxRegions];
    __syncthreads();
    for (int r = threadIdx.x; r < nr; r += blockDim.x) { s_pre[r] = R[r].pre; s_p[r] = R[r].p; }
    __syncthreads();
    unsigned acc = 0;
    auto at = [&](long long i) -> const uint4* {
        int lo = 0, hi = nr - 1;
        while (lo < hi) { const int mid = (lo + hi + 1) >> 1; if (s_pre[mid] <= i) lo = mid; else hi = mid - 1; }
        return s_p[lo] + (i - s_pre[lo]);
    };
    long long i = gtid;
    for (; i + 3 * stride < total; i += 4 * stride) {
        const uint4 a = *at(i), b = *at(i + stride), c = *at(i + 2 * stride), d = *at(i + 3 * stride);
        acc ^= a.x ^ a.y ^ a.z ^ a.w ^ b.x ^ b.y ^ b.z ^ b.w ^ c.x ^ c.y ^ c.z ^ c.w ^ d.x ^ d.y ^ d.z ^ d.w;
    }
    for (; i < total; i += stride) { const uint4 v = *at(i); acc ^= v.x ^ v.y ^ v.z ^ v.w; }
    return acc;
}
__device__ __forceinline__ void fl_barrier(unsigned* bar, unsigned& gen) {
    __syncthreads();
    if (threadIdx.x == 0) {
        gen += gridDim.x;
        __threadfence();
        atomicAdd(bar, 1u);
        while (atomicAdd(bar, 0u) < gen) __builtin_amdgcn_s_sleep(1);
        __threadfence();
    }
    __syncthreads();
}
__global__ void __launch_bounds__(256) floor_persistent(const Phase* ph, int n_ph, const Region* R, const LayerDev* LD,
                                                        int n_rows, long long nx, int nk, unsigned* bar, unsigned* sink,
                                                        unsigned long long* stamps) {
    unsigned gen = 0, acc = 0;
    if (stamps && blockIdx.x == 0 && threadIdx.x == 0) stamps[0] = wall_clock64();
    const long long gtid = (long long) blockIdx.x * blockDim.x + threadIdx.x, stride = (long long) gridDim.x * blockDim.x;
    for (int p = 0; p < n_ph; ++p) {
        const Phase P = ph[p];
        if (P.op != OP_NONE) {
            if (blockIdx.x == 0) fl_op(P.op, LD[P.li], n_rows, nx, nk);
            else if (P.nr > 0) acc ^= fl_read(R + P.r0, P.nr, P.total, gtid - blockDim.x, stride - blockDim.x);
        } else if (P.nr > 0) acc ^= fl_read(R + P.r0, P.nr, P.total, gtid, stride);
        if (p + 1 < n_ph) fl_barrier(bar, gen);
        if (stamps && blockIdx.x == 0 && threadIdx.x == 0) stamps[p + 1] = wall_clock64();
    }
    if (acc == 0x9e3779b9u) *sink = acc;
}
// the same phases as one kernel each (the graph's way), for the comparison
__global__ void __launch_bounds__(256) floor_phase(const Phase* ph, int p, const Region* R, const LayerDev* LD,
                                                   int n_rows, long long nx, int nk, unsigned* sink) {
    const Phase P = ph[p];
    const long long gtid = (long long) blockIdx.x * blockDim.x + threadIdx.x, stride = (long long) gridDim.x * blockDim.x;
    if (P.op != OP_NONE && blockIdx.x == 0) fl_op(P.op, LD[P.li], n_rows, nx, nk);
    unsigned acc = 0;
    if (P.nr > 0) acc = fl_read(R + P.r0, P.nr, P.total, gtid, stride);
    if (acc == 0x9e3779b9u) *sink = acc;
}
}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) {
        std::fprintf(stderr, "usage: ffn_half_bench <shard1.gguf> [shard2.gguf] [layers] [rows] [iters]\n");
        return 2;
    }
    Shards S;
    int ai = 1;
    S.f.emplace_back(new strata::GgufFile(argv[ai++]));
    if (ai < argc && std::strstr(argv[ai], ".gguf")) S.f.emplace_back(new strata::GgufFile(argv[ai++]));
    std::vector<int> layers;
    {
        std::stringstream ss(ai < argc ? argv[ai++] : "1,2,4,5,6,8,9,10");
        std::string t;
        while (std::getline(ss, t, ',')) layers.push_back(std::atoi(t.c_str()));
    }
    const int n = ai < argc ? std::atoi(argv[ai++]) : 2;
    const int iters = ai < argc ? std::atoi(argv[ai++]) : 200;
    if (n < 2 || n > 8) { std::fprintf(stderr, "rows 2..8 (the window's multi-row path)\n"); return 2; }
    const int64_t cap = (int64_t) n * KR, capx = cap;
    const int64_t ptr_off = ((4 + (capx + 1) + 2 * capx) + 1) & ~1ll;
    const int64_t plan_i32 = ptr_off + 4 * cap + (cap + 1) + 1;

    K::shared_expert_set_native_bf16(true);
    cudaStream_t s;
    CK(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking));

    // ---- buffers shared by the layers (as in the Verifier)
    float *logits, *w, *sh_gate, *sh_up, *sh_g, *shared, *hit_out, *bo;
    int32_t* ids;
    void *sh_xq, *scratch;
    uint8_t* stage;
    CK(cudaMalloc((void**) &logits, (size_t) n * NE * 4));
    CK(cudaMalloc((void**) &w, (size_t) n * KR * 4));
    CK(cudaMalloc((void**) &ids, (size_t) n * KR * 4));
    CK(cudaMalloc((void**) &sh_gate, (size_t) n * FF * 4));
    CK(cudaMalloc((void**) &sh_up, (size_t) n * FF * 4));
    CK(cudaMalloc((void**) &sh_g, (size_t) n * 4));
    CK(cudaMalloc((void**) &shared, (size_t) n * N * 4));
    CK(cudaMalloc((void**) &hit_out, (size_t) cap * N * 4));
    CK(cudaMalloc((void**) &bo, (size_t) n * N * 4));
    CK(cudaMalloc(&sh_xq, (size_t) n * N / 32 * 36 * 2));
    CK(cudaMalloc(&scratch, K::native_expert_scratch_bytes(cap, FF)));
    size_t max_blob = 0;
    float *m_x, *m_w, *m_ymiss;
    int32_t* m_ids;
    uint32_t *m_seq, *m_flagA, *m_flagB, *m_flagC;
    mapped<float>((size_t) n * N, &m_x);
    mapped<float>((size_t) n * KR, &m_w);
    mapped<int32_t>((size_t) n * KR, &m_ids);
    mapped<float>((size_t) cap * N, &m_ymiss);
    mapped<uint32_t>(1, &m_seq);
    uint32_t* h_flagA = mapped<uint32_t>(1, &m_flagA);
    uint32_t* h_flagB = mapped<uint32_t>(1, &m_flagB);
    uint32_t* h_flagC = mapped<uint32_t>(1, &m_flagC);
    *h_flagA = *h_flagB = *h_flagC = 0x7fffffffu;   // the pool has answered every ring already

    // ---- the layers
    std::mt19937 rng(1234);
    std::normal_distribution<float> nd(0.f, 1.f);
    std::vector<Layer> Ls;
    for (int l : layers) {
        Layer Y;
        Y.l = l;
        const std::string b = "blk." + std::to_string(l) + ".";
        auto tr = S.find(b + "ffn_gate_inp.weight"), tsgi = S.find(b + "ffn_gate_inp_shexp.weight"),
             tg = S.find(b + "ffn_gate_shexp.weight"), tu = S.find(b + "ffn_up_shexp.weight"),
             td = S.find(b + "ffn_down_shexp.weight"), eg = S.find(b + "ffn_gate_exps.weight"),
             eu = S.find(b + "ffn_up_exps.weight"), ed = S.find(b + "ffn_down_exps.weight");
        if (!tr.second || !tsgi.second || !tg.second || !tu.second || !td.second || !eg.second || !eu.second || !ed.second) {
            std::printf("layer %d: tensors missing (pass both shards?)\n", l);
            return 1;
        }
        Y.router_b = tensor_bytes(*tr.second); Y.sgi_b = tensor_bytes(*tsgi.second);
        Y.sg_b = tensor_bytes(*tg.second); Y.su_b = tensor_bytes(*tu.second); Y.sd_b = tensor_bytes(*td.second);
        Y.router = (uint16_t*) upload(S.data(tr), tensor_bytes(*tr.second));
        Y.sgi = (uint16_t*) upload(S.data(tsgi), tensor_bytes(*tsgi.second));
        Y.sh.gate_type = (int) tg.second->type; Y.sh.gate_data = upload(S.data(tg), tensor_bytes(*tg.second));
        Y.sh.up_type = (int) tu.second->type; Y.sh.up_data = upload(S.data(tu), tensor_bytes(*tu.second));
        Y.sh.down_type = (int) td.second->type; Y.sh.down_data = upload(S.data(td), tensor_bytes(*td.second));
        Y.sh.q8_1 = sh_xq;
        cpu::NativeFmt f;
        std::string err;
        if (!cpu::native_fmt((int) eg.second->type, (int) ed.second->type, N, FF, f, err)) {
            std::printf("layer %d: %s\n", l, err.c_str());
            return 1;
        }
        Y.L = K::native_expert_layout(f.gu_type, f.d_type, N, FF);
        Y.blob = f.bytes;
        Y.down_off = f.down_off;
        if (f.bytes > max_blob) max_blob = f.bytes;
        // the FFN input rows: unit-variance rows, routed by the layer's real router
        std::vector<float> x((size_t) n * N);
        for (auto& v : x) v = nd(rng);
        Y.x = (float*) upload(x.data(), x.size() * 4);
        CK(cudaMalloc(&Y.xq, (size_t) n * N / 32 * 36));
        K::quantize_q8_1_rows(Y.x, n, N, Y.xq, s);
        K::bf16_gemv_fp32_mmvf_multi(Y.x, N, Y.router, logits, NE, N, NE, n, s);
        K::native_router_top10_multi(logits, ids, w, n, s);
        CK(cudaMalloc((void**) &Y.logits, (size_t) n * NE * 4));
        CK(cudaMemcpyAsync(Y.logits, logits, (size_t) n * NE * 4, cudaMemcpyDeviceToDevice, s));
        std::vector<int32_t> hid((size_t) cap);
        CK(cudaMemcpyAsync(hid.data(), ids, (size_t) cap * 4, cudaMemcpyDeviceToHost, s));
        CK(cudaStreamSynchronize(s));
        // the pool's plan with every expert resident: distinct experts in routing order, entries ascending
        std::vector<int64_t> first_of((size_t) cap);
        std::vector<int64_t> distinct;
        for (int64_t i = 0; i < cap; ++i) {
            first_of[(size_t) i] = i;
            for (int64_t j = 0; j < i; ++j)
                if (hid[(size_t) j] == hid[(size_t) i]) { first_of[(size_t) i] = first_of[(size_t) j]; break; }
            if (first_of[(size_t) i] == i) distinct.push_back(i);
        }
        CK(cudaMalloc((void**) &Y.blobs, distinct.size() * f.bytes));
        const uint8_t *pg = S.data(eg), *pu = S.data(eu), *pd = S.data(ed);
        const size_t dsz = f.bytes - f.down_off, drow = dsz / N;
        std::vector<uint8_t> blob(f.bytes);
        Y.h_plan = mapped<int32_t>((size_t) plan_i32 + 16, &Y.m_plan);
        CK(cudaMalloc((void**) &Y.plan, ((size_t) plan_i32 + 16) * 4));
        int32_t* P = Y.h_plan;
        int32_t* start = P + 4;
        int32_t* dst = start + capx + 1;
        int32_t* tok = dst + capx;
        unsigned long long* ptr = (unsigned long long*) (P + ptr_off);
        int32_t* start2 = P + ptr_off + 4 * capx;
        int groups = 0, entries = 0;
        for (int64_t i0 : distinct) {
            const size_t E = (size_t) hid[(size_t) i0];
            std::memcpy(blob.data(), pg + E * f.up_off, f.up_off);
            std::memcpy(blob.data() + f.up_off, pu + E * f.up_off, f.up_off);
            for (int64_t r = 0; r < N; ++r)
                std::memcpy(blob.data() + f.down_off + r * drow, pd + E * dsz + r * drow, drow);
            uint8_t* dev = Y.blobs + (size_t) groups * f.bytes;
            CK(cudaMemcpy(dev, blob.data(), f.bytes, cudaMemcpyHostToDevice));
            ptr[groups] = (unsigned long long) dev;
            start[groups] = entries;
            for (int64_t i = i0; i < cap; ++i)
                if (first_of[(size_t) i] == i0) { dst[entries] = (int32_t) i; tok[entries] = (int32_t) (i / KR); ++entries; }
            ++groups;
        }
        start[groups] = entries;
        start2[0] = entries;
        P[0] = groups; P[1] = entries; P[2] = 0;
        Y.groups = groups;
        Ls.push_back(Y);
        std::printf("layer %2d: shared %s/%s/%s, experts %s/%s, %d distinct of %lld routed\n", l,
                    ggml_type_name((ggml_type) Y.sh.gate_type), ggml_type_name((ggml_type) Y.sh.up_type),
                    ggml_type_name((ggml_type) Y.sh.down_type), ggml_type_name((ggml_type) f.gu_type),
                    ggml_type_name((ggml_type) f.d_type), groups, (long long) cap);
    }
    CK(cudaMalloc((void**) &stage, 16 * max_blob));

    // ---- the chain, as record_window captures it (production HIP: always_publish, gather, no fork, QFUSE)
    auto chain = [&](const Layer& Y) {
        int32_t* pl = Y.plan;
        const int32_t* p_counts = pl;
        const int32_t* p_start = pl + 4;
        const int32_t* p_dst = p_start + capx + 1;
        const int32_t* p_tok = p_dst + capx;
        const unsigned long long* p_ptr = (const unsigned long long*) (pl + ptr_off);
        const unsigned long long* p_ptr2 = p_ptr + capx;
        const int32_t* p_start2 = pl + ptr_off + 4 * capx;
        K::bf16_gemv_fp32_mmvf_multi(Y.x, N, Y.router, logits, NE, N, NE, n, s);
        K::native_router_top10_multi(logits, ids, w, n, s);
        K::doorbell_publish(Y.x, ids, w, (int64_t) n * N, (int64_t) n * KR, m_x, m_ids, m_w, m_seq, s);
        K::shared_expert_multi(n, Y.x, nullptr, Y.sh, Y.sgi, sh_gate, sh_up, sh_g, shared, N, FF, s, nullptr, 0);
        K::wait_flag_ge(m_flagA, 1, s);
        K::copy_i32_from_mapped(pl, Y.m_plan, plan_i32, s);
        K::native_expert_grouped(Y.L, p_ptr, p_start, p_counts, p_dst, p_tok, cap, cap, Y.xq, scratch, hit_out, s, 0);
        K::wait_flag_ge(m_flagB, 1, s);
        K::fetch_blobs(p_ptr2, p_counts + 2, stage, (int64_t) Y.blob, 16, s);
        K::rebase_ptrs((unsigned long long*) p_ptr2, p_counts + 2, stage, (int64_t) Y.blob, s);
        K::native_expert_grouped(Y.L, p_ptr2, p_start2, p_counts + 2, p_dst, p_tok, cap, cap, Y.xq, scratch, hit_out, s, 4);
        K::wait_flag_ge(m_flagC, 1, s);
        K::native_moe_combine_gather_multi(m_ymiss, hit_out, p_dst, p_counts + 1, w, shared, bo, N, KR, n, s);
    };

    cudaGraph_t g;
    cudaGraphExec_t ge;
    CK(cudaStreamBeginCapture(s, cudaStreamCaptureModeThreadLocal));
    for (const auto& Y : Ls) chain(Y);
    CK(cudaStreamEndCapture(s, &g));
    size_t nodes = 0;
    CK(cudaGraphGetNodes(g, nullptr, &nodes));
    CK(cudaGraphInstantiate(&ge, g, nullptr, nullptr, 0));
    cudaEvent_t e0, e1;
    CK(cudaEventCreate(&e0));
    CK(cudaEventCreate(&e1));
    for (int i = 0; i < 20; ++i) CK(cudaGraphLaunch(ge, s));
    CK(cudaStreamSynchronize(s));
    float best = 1e30f, sum = 0;
    const int rounds = 5;
    for (int r = 0; r < rounds; ++r) {
        CK(cudaEventRecord(e0, s));
        for (int i = 0; i < iters; ++i) CK(cudaGraphLaunch(ge, s));
        CK(cudaEventRecord(e1, s));
        CK(cudaEventSynchronize(e1));
        float ms = 0;
        CK(cudaEventElapsedTime(&ms, e0, e1));
        ms /= iters;
        sum += ms;
        if (ms < best) best = ms;
    }
    std::vector<float> out((size_t) n * N);
    CK(cudaMemcpy(out.data(), bo, out.size() * 4, cudaMemcpyDeviceToHost));
    unsigned long long hsum = 1469598103934665603ull;
    for (float v : out) { uint32_t u; std::memcpy(&u, &v, 4); hsum = (hsum ^ u) * 1099511628211ull; }
    std::printf("chain: %zu layers, %zu graph nodes (%.1f per layer), rows %d: best %.1f us per layer, mean %.1f "
                "(last layer's output hash %016llx)\n", Ls.size(), nodes, (double) nodes / Ls.size(), n,
                1e3 * best / Ls.size(), 1e3 * sum / rounds / Ls.size(), hsum);

    // ---- the floor (see floor_persistent): phases per layer, as a persistent kernel and as one kernel each
    std::vector<Region> R;
    std::vector<Phase> PH;
    std::vector<LayerDev> LDh;
    auto region = [&](const void* p, size_t bytes) {
        const uintptr_t a = ((uintptr_t) p + 15) & ~(uintptr_t) 15;
        const long long nv = (long long) ((bytes - (a - (uintptr_t) p)) / 16);
        R.push_back(Region{(const uint4*) a, nv, 0});
    };
    float *f_gate_out, *f_up_out;
    CK(cudaMalloc((void**) &f_gate_out, (size_t) cap * FF * 4));
    CK(cudaMalloc((void**) &f_up_out, (size_t) cap * FF * 4));
    for (size_t li = 0; li < Ls.size(); ++li) {
        const Layer& Y = Ls[li];
        LDh.push_back(LayerDev{Y.logits, ids, w, Y.x, m_x, m_ids, m_w, m_seq, m_flagA, m_flagB, m_flagC, Y.m_plan,
                               Y.plan, (int) plan_i32});
        auto phase = [&](int op, std::initializer_list<std::pair<const void*, size_t>> regs) {
            Phase P{op, (int) R.size(), 0, 0, (int) li};
            for (const auto& r : regs) region(r.first, r.second);
            P.nr = (int) R.size() - P.r0;
            long long pre = 0;
            for (int i = P.r0; i < (int) R.size(); ++i) { R[(size_t) i].pre = pre; pre += R[(size_t) i].n; }
            P.total = pre;
            PH.push_back(P);
        };
        const size_t nx = (size_t) n * N * 4;
        phase(OP_NONE, {{Y.router, Y.router_b}, {Y.x, nx}});                            // router GEMV
        phase(OP_TOP10, {{Y.sh.gate_data, Y.sg_b}, {Y.sh.up_data, Y.su_b}});           // top-10 | shared gate + up
        phase(OP_DOORBELL, {{sh_gate, (size_t) n * FF * 4}, {sh_up, (size_t) n * FF * 4}});   // doorbell | shared swiglu
        phase(OP_NONE, {{Y.sh.down_data, Y.sd_b}, {Y.sgi, Y.sgi_b}});                   // shared down, its gate row
        phase(OP_WAIT_PLAN, {{shared, nx}});                                            // wait A + plan | sigmoid scale
        {   // the VRAM experts' gate/up rows, then (after their swiglu) their down rows
            Phase P{OP_NONE, (int) R.size(), 0, 0, (int) li};
            for (int gi = 0; gi < Y.groups; ++gi) region(Y.blobs + (size_t) gi * Y.blob, Y.down_off);
            P.nr = (int) R.size() - P.r0;
            long long pre = 0;
            for (int i = P.r0; i < (int) R.size(); ++i) { R[(size_t) i].pre = pre; pre += R[(size_t) i].n; }
            P.total = pre; PH.push_back(P);
        }
        phase(OP_NONE, {{f_gate_out, (size_t) cap * FF * 4}, {f_up_out, (size_t) cap * FF * 4}});   // swiglu entries
        {
            Phase P{OP_NONE, (int) R.size(), 0, 0, (int) li};
            for (int gi = 0; gi < Y.groups; ++gi)
                region(Y.blobs + (size_t) gi * Y.blob + Y.down_off, Y.blob - Y.down_off);
            P.nr = (int) R.size() - P.r0;
            long long pre = 0;
            for (int i = P.r0; i < (int) R.size(); ++i) { R[(size_t) i].pre = pre; pre += R[(size_t) i].n; }
            P.total = pre; PH.push_back(P);
        }
        phase(OP_WAIT2, {{hit_out, (size_t) cap * N * 4}, {shared, nx}});              // waits B, C + combine
    }
    Phase* d_ph; Region* d_R; LayerDev* d_LD; unsigned *bar, *sink;
    CK(cudaMalloc((void**) &d_ph, PH.size() * sizeof(Phase)));
    CK(cudaMemcpy(d_ph, PH.data(), PH.size() * sizeof(Phase), cudaMemcpyHostToDevice));
    CK(cudaMalloc((void**) &d_R, R.size() * sizeof(Region)));
    CK(cudaMemcpy(d_R, R.data(), R.size() * sizeof(Region), cudaMemcpyHostToDevice));
    CK(cudaMalloc((void**) &d_LD, LDh.size() * sizeof(LayerDev)));
    CK(cudaMemcpy(d_LD, LDh.data(), LDh.size() * sizeof(LayerDev), cudaMemcpyHostToDevice));
    CK(cudaMalloc((void**) &bar, 4));
    CK(cudaMalloc((void**) &sink, 4));
    long long bytes = 0;
    for (const auto& r : R) bytes += r.n * 16;
    int per_cu = 0, dev = 0;
    CK(cudaGetDevice(&dev));
    cudaDeviceProp prop;
    CK(cudaGetDeviceProperties(&prop, dev));
    CK(hipOccupancyMaxActiveBlocksPerMultiprocessor(&per_cu, floor_persistent, 256, 0));   // (no shim name for it)
    const int resident = per_cu * prop.multiProcessorCount;
    auto timeit = [&](auto f) {
        for (int i = 0; i < 10; ++i) f();
        CK(cudaStreamSynchronize(s));
        float best = 1e30f;
        for (int r = 0; r < 5; ++r) {
            CK(cudaEventRecord(e0, s));
            for (int i = 0; i < iters; ++i) f();
            CK(cudaEventRecord(e1, s));
            CK(cudaEventSynchronize(e1));
            float ms = 0;
            CK(cudaEventElapsedTime(&ms, e0, e1));
            if (ms / iters < best) best = ms / iters;
        }
        return 1e3f * best / (float) Ls.size();
    };
    std::printf("floor: %zu phases per layer, %.1f MB read per layer; %d blocks of 256 resident at most\n",
                PH.size() / Ls.size(), bytes / 1e6 / Ls.size(), resident);
    for (int blocks : {96, 192, 256, 320, 384}) {
        if (blocks > resident) continue;
        const float t = timeit([&] {
            CK(cudaMemsetAsync(bar, 0, 4, s));
            floor_persistent<<<blocks, 256, 0, s>>>(d_ph, (int) PH.size(), d_R, d_LD, n, (long long) n * N, n * (int) KR, bar, sink, nullptr);
        });
        std::printf("  persistent, %3d blocks: %6.1f us per layer (%.0f GB/s over the layer)\n", blocks, t,
                    bytes / Ls.size() / (t * 1e3));
        // where the time goes: block 0's clock after each phase (wall_clock64, 100 MHz), 50 runs averaged
        unsigned long long* st;
        CK(cudaMalloc((void**) &st, (PH.size() + 1) * 8));
        std::vector<unsigned long long> hs(PH.size() + 1);
        const int per = (int) (PH.size() / Ls.size());
        std::vector<double> acc_us((size_t) per, 0.0);
        const int runs = 50;
        for (int r = 0; r < runs; ++r) {
            CK(cudaMemsetAsync(bar, 0, 4, s));
            floor_persistent<<<blocks, 256, 0, s>>>(d_ph, (int) PH.size(), d_R, d_LD, n, (long long) n * N, n * (int) KR, bar, sink, st);
            CK(cudaMemcpyAsync(hs.data(), st, hs.size() * 8, cudaMemcpyDeviceToHost, s));
            CK(cudaStreamSynchronize(s));
            for (size_t p = 0; p < PH.size(); ++p) acc_us[p % per] += (hs[p + 1] - hs[p]) / 100.0;   // 100 MHz ticks -> us
        }
        const char* names[] = {"router read", "top-10 | shared gate+up", "doorbell | shared swiglu", "shared down read", "wait A + plan | scale", "experts gate/up read", "swiglu entries",
                               "experts down read", "waits B, C + combine"};
        for (int p = 0; p < per; ++p) {
            const double us = acc_us[(size_t) p] / runs / Ls.size();
            long long b = 0;
            for (size_t q = (size_t) p; q < PH.size(); q += per) b += PH[q].total * 16;
            const double mb = b / 1e6 / Ls.size();
            std::printf("      %-26s %6.2f us  %6.2f MB  %6.0f GB/s\n", p < 9 ? names[p] : "?", us, mb, us > 0 ? mb * 1e3 / us : 0.0);
        }
        CK(cudaFree(st));
    }
    {
        cudaGraph_t g2;
        cudaGraphExec_t ge2;
        CK(cudaStreamBeginCapture(s, cudaStreamCaptureModeThreadLocal));
        for (int p = 0; p < (int) PH.size(); ++p)
            floor_phase<<<PH[(size_t) p].nr > 0 ? 320 : 1, 256, 0, s>>>(d_ph, p, d_R, d_LD, n, (long long) n * N, n * (int) KR, sink);
        CK(cudaStreamEndCapture(s, &g2));
        CK(cudaGraphInstantiate(&ge2, g2, nullptr, nullptr, 0));
        const float t = timeit([&] { CK(cudaGraphLaunch(ge2, s)); });
        std::printf("  the same phases as one kernel each (graph, 320 blocks): %6.1f us per layer\n", t);
    }
    return 0;
}
