// native_router_top10_multi (the engine's top-10 over 512 router logits, 2 rows) timed in a graph chain of 400
// launches against an empty launch of the same shape: what the kernel itself costs per layer.
#include "strata/kernels/native_router.hpp"
#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#define CK(x) do { cudaError_t e_ = (x); if (e_ != cudaSuccess) { std::printf("%s: %s\n", #x, cudaGetErrorString(e_)); std::exit(1); } } while (0)
__global__ void empty_kernel(int* p) { if (threadIdx.x == 999) *p = 1; }
int main() {
    const int n = 2, N = 400;
    std::mt19937 rng(7); std::normal_distribution<float> nd(0.f, 2.f);
    std::vector<float> h(n * 512); for (auto& v : h) v = nd(rng);
    float *lg, *w; int* ids;
    CK(cudaMalloc((void**) &lg, h.size() * 4)); CK(cudaMemcpy(lg, h.data(), h.size() * 4, cudaMemcpyHostToDevice));
    CK(cudaMalloc((void**) &w, n * 10 * 4)); CK(cudaMalloc((void**) &ids, n * 10 * 4));
    cudaStream_t s; CK(cudaStreamCreate(&s));
    cudaEvent_t a, b; CK(cudaEventCreate(&a)); CK(cudaEventCreate(&b));
    auto run = [&](const char* name, auto f) {
        cudaGraph_t g; cudaGraphExec_t ge;
        CK(cudaStreamBeginCapture(s, cudaStreamCaptureModeThreadLocal));
        for (int i = 0; i < N; ++i) f();
        CK(cudaStreamEndCapture(s, &g)); CK(cudaGraphInstantiate(&ge, g, nullptr, nullptr, 0));
        CK(cudaGraphLaunch(ge, s)); CK(cudaStreamSynchronize(s));
        float best = 1e9;
        for (int r = 0; r < 5; ++r) {
            CK(cudaEventRecord(a, s)); CK(cudaGraphLaunch(ge, s)); CK(cudaEventRecord(b, s)); CK(cudaEventSynchronize(b));
            float ms; CK(cudaEventElapsedTime(&ms, a, b)); if (ms < best) best = ms;
        }
        std::printf("  %-40s %.2f us per launch\n", name, best * 1000 / N);
    };
    strata::kernels::native_router_set_enabled(true);
    run("native_router_top10_multi, 2 rows", [&] { strata::kernels::native_router_top10_multi(lg, ids, w, n, s); });
    run("empty kernel, 1 x (32,8)", [&] { empty_kernel<<<1, dim3(32, 8), 0, s>>>(ids); });
    return 0;
}
