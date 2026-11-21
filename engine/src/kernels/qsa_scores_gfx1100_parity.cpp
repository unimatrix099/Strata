// GPU contract test for the optional gfx1100 port of PR1825. Synthetic, no model or coding score.
#include "strata/kernels/qsa.hpp"
#include "strata/kernels/qsa_select.hpp"
#include "strata/kernels/gfx_arch.hpp"
#include <cuda_runtime.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

namespace strata::kernels {
// Test-only host wrapper copied from the unchanged baseline; both arms receive the SAME active bound.
void qsa_block_scores_reference(const float*, const float*, const float*, const int32_t*, int64_t,
                                int64_t, const QsaShapes&, float*, void*, int64_t);
}
namespace k = strata::kernels;
namespace {
void ck(cudaError_t e) {
    if (e != cudaSuccess) { std::fprintf(stderr, "HIP: %s\n", cudaGetErrorString(e)); std::exit(2); }
}
void require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
struct Buffer {
    void* p = nullptr; size_t bytes;
    explicit Buffer(size_t n): bytes(n) { ck(cudaMalloc(&p, n + 64)); ck(cudaMemset(p, 0xd3, n + 64)); }
    ~Buffer() { ck(cudaFree(p)); }
    template<typename T> T* as() { return static_cast<T*>(p); }
    void guard() {
        unsigned char v[64];
        ck(cudaMemcpy(v, static_cast<char*>(p) + bytes, 64, cudaMemcpyDeviceToHost));
        for (auto c: v) require(c == 0xd3, "allocation guard modified");
    }
};
struct Case { int64_t context, queries, capacity; int mode; int bound; bool timing; };
int cases_done = 0, engaged = 0;
bool g_long = false;   // --dec: the candidate is the long-context decode select (qsa_block_scores_long / qsa_block_topk_long)
uint64_t scores_checked = 0, ids_checked = 0;

void check(Case c) {
    const auto s = k::qsa_real_shapes();
    const int64_t nb = c.capacity / s.idx_block + 2, nq = c.queries;
    const int64_t active = c.context / s.idx_block + 1;
    const int64_t bound = c.bound == 0 ? active : c.bound == 1 ? 0 : c.bound == 2 ? -1 : nb + 1;
    const int64_t cap = k::qsa_selection_width(k::kTopkMaxCells, s);
    std::mt19937 rng(91731 + (unsigned)c.context + 7*(unsigned)nq + 101*c.mode);
    std::normal_distribution<float> nd(0, 1);
    std::uniform_int_distribution<int> expo(-20, 20), small(-3, 3);
    uint64_t sequence = 0;
    auto value = [&]() -> float {
        ++sequence;
        if (c.mode == 1) return sequence % 11 == 0 ? 0.f : std::ldexp(nd(rng), expo(rng));
        if (c.mode == 2) return (float)small(rng);
        if (c.mode == 3) return sequence % 2 ? 0.f : -0.f;
        if (c.mode == 4) {
            if (sequence % 23 == 0) return std::numeric_limits<float>::quiet_NaN();
            if (sequence % 29 == 0) return std::numeric_limits<float>::infinity();
            if (sequence % 31 == 0) return -std::numeric_limits<float>::infinity();
        }
        if (c.mode == 5) return sequence % 4 < 2 ? std::ldexp(1.f, 20) : -std::ldexp(1.f, 20);
        return nd(rng);
    };
    std::vector<float> pooled(nb*s.idx_dim), dead(s.idx_dim), q(nq*s.idx_n_head*s.idx_dim);
    for (auto& v: pooled) v = value();
    for (auto& v: dead) v = value();
    for (auto& v: q) v = value();
    // Padding after the largest live block must never affect scores or selected IDs.
    for (int64_t i = active * s.idx_dim; i < (int64_t)pooled.size(); ++i)
        pooled[i] = std::numeric_limits<float>::quiet_NaN();
    std::vector<int32_t> steps(nq*k::kStepCount);
    for (int64_t i = 0; i < nq; ++i) {
        auto* st = steps.data() + i*k::kStepCount;
        const int64_t n = c.context - nq + i + 1;
        st[k::kStepPos] = n-1; st[k::kStepNKv] = n;
        st[k::kStepNBid] = n/s.idx_block;
        st[k::kStepWidth] = k::qsa_selection_width(n, s);
    }
    Buffer p(pooled.size()*4), d(dead.size()*4), query(q.size()*4), step(steps.size()*4);
    Buffer a(nq*nb*4), b(nq*nb*4), ia(nq*cap*4), ib(nq*cap*4);
    ck(cudaMemcpy(p.p, pooled.data(), p.bytes, cudaMemcpyHostToDevice));
    ck(cudaMemcpy(d.p, dead.data(), d.bytes, cudaMemcpyHostToDevice));
    ck(cudaMemcpy(query.p, q.data(), query.bytes, cudaMemcpyHostToDevice));
    ck(cudaMemcpy(step.p, steps.data(), step.bytes, cudaMemcpyHostToDevice));
    auto reference = [&] {
        k::qsa_block_scores_reference(p.as<float>(), d.as<float>(), query.as<float>(), step.as<int32_t>(),
                                     nq, nb, s, a.as<float>(), nullptr, bound);
    };
    Buffer scratch((size_t) std::max<int64_t>(k::qsa_topk_multi_scratch_ints(nq), 1) * 4);
    auto candidate = [&] {
        if (g_long) k::qsa_block_scores_long(p.as<float>(), d.as<float>(), query.as<float>(), step.as<int32_t>(),
                                             nq, nb, s, b.as<float>(), nullptr, bound);
        else k::qsa_block_scores(p.as<float>(), d.as<float>(), query.as<float>(), step.as<int32_t>(),
                           nq, nb, s, b.as<float>(), nullptr, bound);
    };

    ck(cudaMemset(a.p, 0xa5, a.bytes)); ck(cudaMemset(b.p, 0x5a, b.bytes));
    reference(); candidate(); ck(cudaDeviceSynchronize());
    std::vector<uint32_t> av(nq*nb), bv(nq*nb);
    ck(cudaMemcpy(av.data(), a.p, a.bytes, cudaMemcpyDeviceToHost));
    ck(cudaMemcpy(bv.data(), b.p, b.bytes, cudaMemcpyDeviceToHost));
    uint64_t checked = 0;
    for (int64_t i = 0; i < nq; ++i) {
        const int64_t last = steps[i*k::kStepCount+k::kStepNBid];
        for (int64_t j = 0; j < nb; ++j) {
            const auto at = i*nb+j;
            if (j <= last) {
                if (av[at] != bv[at]) {
                    std::fprintf(stderr, "MISMATCH ctx=%lld nq=%lld mode=%d query=%lld block=%lld bits=%08x/%08x\n",
                        (long long)c.context, (long long)nq, c.mode, (long long)i, (long long)j, av[at], bv[at]);
                    require(false, "scores differ bitwise");
                }
                ++checked;
            } else {
                require(av[at] == 0xa5a5a5a5 && bv[at] == 0x5a5a5a5a, "inactive score padding overwritten");
            }
        }
    }
    ck(cudaMemset(ia.p, 0xff, ia.bytes)); ck(cudaMemset(ib.p, 0xff, ib.bytes));
    // STRATA_TOPK_REF=1: the reference's ids come from the original radix kernel (checks the pick kernel as well as the scorer)
    if (std::getenv("STRATA_TOPK_REF")) k::qsa_block_topk_ref(a.as<float>(), step.as<int32_t>(), nq, nb, cap, s, ia.as<int32_t>(), nullptr);
    else k::qsa_block_topk(a.as<float>(), step.as<int32_t>(), nq, nb, cap, s, ia.as<int32_t>(), nullptr, bound);
    if (g_long) k::qsa_block_topk_long(b.as<float>(), step.as<int32_t>(), nq, nb, cap, s, ib.as<int32_t>(), nullptr, bound, scratch.as<int32_t>());
    else k::qsa_block_topk(b.as<float>(), step.as<int32_t>(), nq, nb, cap, s, ib.as<int32_t>(), nullptr, bound);
    ck(cudaDeviceSynchronize());
    std::vector<int32_t> iav(nq*cap), ibv(nq*cap);
    ck(cudaMemcpy(iav.data(), ia.p, ia.bytes, cudaMemcpyDeviceToHost));
    ck(cudaMemcpy(ibv.data(), ib.p, ib.bytes, cudaMemcpyDeviceToHost));
    require(iav == ibv, "selected IDs differ");
    for (int64_t i = 0; i < nq; ++i)
        for (int64_t j = steps[i*k::kStepCount+k::kStepWidth]; j < cap; ++j)
            require(iav[i*cap+j] == -1, "ID padding overwritten");
    for (auto* v: {&p, &d, &query, &step, &a, &b, &ia, &ib}) v->guard();
    const bool branch = std::getenv("STRATA_SCORES_THREAD") &&
        std::atoi(std::getenv("STRATA_SCORES_THREAD")) == 1 && bound >= 16385 && bound <= nb && nq >= 64;
    std::printf("PASS ctx=%lld nq=%lld capacity=%lld mode=%d bound=%lld expected_thread=%d scores=%llu ids=%lld\n",
                (long long)c.context, (long long)nq, (long long)c.capacity, c.mode, (long long)bound,
                (int)branch, (unsigned long long)checked, (long long)(nq*cap));
    ++cases_done; engaged += branch; scores_checked += checked; ids_checked += nq*cap;
    if (c.timing) {
        // Same actual bound/capacity, warmed, interleaved ABBA. Events measure GPU execution, not host fixture setup.
        for (int i = 0; i < 3; ++i) { reference(); candidate(); }
        ck(cudaDeviceSynchronize());
        cudaEvent_t e0, e1; ck(cudaEventCreate(&e0)); ck(cudaEventCreate(&e1));
        float times[2][4]{};
        for (int batch = 0; batch < 4; ++batch) {
            const int order[4] = {0,1,1,0};
            for (int v: order) {
                ck(cudaEventRecord(e0));
                for (int rep = 0; rep < 5; ++rep) { if (v == 0) reference(); else candidate(); }
                ck(cudaEventRecord(e1)); ck(cudaEventSynchronize(e1));
                float ms = 0; ck(cudaEventElapsedTime(&ms, e0, e1)); times[v][batch] += ms/10;
            }
        }
        float ta=0, tb=0;
        for (int i=0; i<4; ++i) { ta+=times[0][i]/4; tb+=times[1][i]/4; }
        std::printf("TIMING ctx=%lld nq=%lld baseline_ms=%.6f candidate_ms=%.6f speedup=%.6f trials=4 repeats_per_trial=10\n",
                    (long long)c.context, (long long)nq, ta, tb, ta/tb);
        // the pick that follows the scorer (qsa_block_topk on the reference's scores), same bound
        float tk[2] = {0, 0};
        for (int v = 0; v < 2; ++v) {   // 0: qsa_block_topk, 1: the long-context pick
            auto run = [&] {
                if (v == 1) k::qsa_block_topk_long(a.as<float>(), step.as<int32_t>(), nq, nb, cap, s, ia.as<int32_t>(), nullptr, bound, scratch.as<int32_t>());
                else k::qsa_block_topk(a.as<float>(), step.as<int32_t>(), nq, nb, cap, s, ia.as<int32_t>(), nullptr, bound);
            };
            for (int i = 0; i < 3; ++i) run();
            ck(cudaDeviceSynchronize());
            ck(cudaEventRecord(e0));
            for (int rep = 0; rep < 20; ++rep) run();
            ck(cudaEventRecord(e1)); ck(cudaEventSynchronize(e1));
            ck(cudaEventElapsedTime(&tk[v], e0, e1));
        }
        std::printf("TOPK ctx=%lld nq=%lld capacity=%lld topk_ms=%.6f long_ms=%.6f\n", (long long)c.context, (long long)nq, (long long)c.capacity, tk[0]/20, tk[1]/20);
        ck(cudaEventDestroy(e0)); ck(cudaEventDestroy(e1));
    }
}
} // namespace

int main(int argc, char** argv) {
    int devices = 0;
    if (cudaGetDeviceCount(&devices) != cudaSuccess || devices == 0) { std::puts("SKIP no GPU"); return 77; }
    cudaDeviceProp properties{}; ck(cudaGetDeviceProperties(&properties, 0));
    if (!k::gfx_arch_is(properties.gcnArchName, "gfx1100") && !k::gfx_arch_is_gfx12(properties.gcnArchName)) { std::puts("SKIP requires gfx1100 or gfx12"); return 77; }
    require(std::getenv("STRATA_SCORES_THREAD") && std::strcmp(std::getenv("STRATA_SCORES_THREAD"), "1") == 0,
            "run with STRATA_SCORES_THREAD=1 so the candidate branch is tested");
    const bool full = argc > 1 && std::strcmp(argv[1], "--full") == 0;
    // --dec: the decode window's calls (no active count, 1..8 queries) through the long-context select (STRATA_SELECT_LONG's
    // kernels) against the multi kernel's scores and, with STRATA_TOPK_REF=1, the original pick's ids, bit for bit; and their times
    if (argc > 1 && std::strcmp(argv[1], "--dec") == 0) {
        g_long = true;
        for (int64_t n: {1,2,3,4,5,6,7,8})
            for (int64_t context: {9,100,103,8000,65537,131075,240001,262140}) if (context >= n) check({context,n,262144,0,2,false});
        for (int mode: {1,2,3,4,5}) check({131075,4,262144,mode,2,false});
        for (int64_t n: {1,2,4}) for (int64_t context: {8000,131075,240000}) check({context,n,262144,0,2,true});
        for (int64_t n: {2,4}) for (int64_t context: {8000,120000}) check({context,n,131072,0,2,true});
        std::printf("DEC cases=%d scores=%llu ids=%llu bitwise_scores=true bitwise_ids=true\n", cases_done,
                    (unsigned long long)scores_checked, (unsigned long long)ids_checked);
        return 0;
    }
    for (int64_t n: {1,6,8,9,63,64,65,127,128,129,256})
        check({131075,n,262144,0,0,false});
    for (int64_t context: {65535,65536,65537,65538,65539,131072,262144})
        check({context,65,262144,0,0,false});
    for (int bound: {1,2,3}) check({131075,65,262144,0,bound,false});
    for (int mode: {1,2,3,4,5}) check({131075,129,262144,mode,0,false});
    check({131075,65,131075,0,0,false});
    if (full) {
        for (int64_t context: {131072,131073,131074,131075}) check({context,256,262144,2,0,false});
        check({131072,256,262144,0,0,true});
        check({240000,256,262144,0,0,true});
        check({140000,1024,262144,0,0,true});
        check({240000,1024,262144,0,0,true});
    }
    std::printf("QUALIFIED cases=%d expected_thread_cases=%d scores=%llu ids=%llu bitwise_scores=true bitwise_ids=true guards=true\n",
                cases_done, engaged, (unsigned long long)scores_checked, (unsigned long long)ids_checked);
    return 0;
}
