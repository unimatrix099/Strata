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
};

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
        if (f.bytes > max_blob) max_blob = f.bytes;
        // the FFN input rows: unit-variance rows, routed by the layer's real router
        std::vector<float> x((size_t) n * N);
        for (auto& v : x) v = nd(rng);
        Y.x = (float*) upload(x.data(), x.size() * 4);
        CK(cudaMalloc(&Y.xq, (size_t) n * N / 32 * 36));
        K::quantize_q8_1_rows(Y.x, n, N, Y.xq, s);
        K::bf16_gemv_fp32_mmvf_multi(Y.x, N, Y.router, logits, NE, N, NE, n, s);
        K::native_router_top10_multi(logits, ids, w, n, s);
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
    return 0;
}
