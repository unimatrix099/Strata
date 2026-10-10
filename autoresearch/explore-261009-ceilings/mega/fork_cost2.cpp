// Why did the engine's shared-expert fork cost 30% when fork_cost.cpp saw none?  The same per-layer fork/join at the
// engine's scale: 27 layers x 39 kernels (1,053 nodes), 7 of them on the side branch, with the main chain made of
// 2-MB kernels and 1-block tiny kernels in the window's proportions (about 60% small), and a variant whose side
// branch is forked but joined only at the end of the next layer (the engine's join comes before the combine).
// hipcc -O2 --offload-arch=gfx1100 fork_cost2.cpp -o fork_cost2 && ./fork_cost2 1
#include <hip/hip_runtime.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); exit(1); } } while (0)
__global__ void slice_kernel(const uint4* p, size_t n_vec, unsigned* out) {
    unsigned acc = 0;
    for (size_t i = blockIdx.x * (size_t) blockDim.x + threadIdx.x; i < n_vec; i += (size_t) gridDim.x * blockDim.x) { const uint4 v = p[i]; acc ^= v.x ^ v.y ^ v.z ^ v.w; }
    if (acc == 0x12345678u) *out = acc;
}
int main(int argc, char** argv) {
    const int dev = argc > 1 ? atoi(argv[1]) : 0;
    CK(hipSetDevice(dev));
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int L = 27, per_layer = 39, side = 7, N = L * per_layer;
    const size_t bytes = 2 << 20, n_vec = bytes / 16;
    const int n_big = 150;   // 300 MB of distinct 2-MB slices, reused round-robin
    uint4* buf; unsigned* out;
    CK(hipMalloc(&buf, bytes * n_big)); CK(hipMemset(buf, 1, bytes * n_big)); CK(hipMalloc(&out, 4));
    hipStream_t s, s2; CK(hipStreamCreate(&s)); CK(hipStreamCreate(&s2));
    hipEvent_t a, b, fork, join; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
    CK(hipEventCreateWithFlags(&fork, hipEventDisableTiming)); CK(hipEventCreateWithFlags(&join, hipEventDisableTiming));
    float ms;
    auto timed = [&](auto f, int reps) { f(); CK(hipStreamSynchronize(s)); CK(hipEventRecord(a, s)); for (int i = 0; i < reps; ++i) f(); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b)); CK(hipEventElapsedTime(&ms, a, b)); return ms / reps; };
    int slice = 0;
    // main-chain kernel k of a layer: big (192 blocks, 2 MB) when k % 5 < 2, else tiny (1 block, 64 KB)
    auto main_kernel = [&](int k, hipStream_t st) {
        const uint4* p = buf + (size_t) (slice++ % n_big) * n_vec;
        if (k % 5 < 2) slice_kernel<<<192, 256, 0, st>>>(p, n_vec, out); else slice_kernel<<<1, 256, 0, st>>>(p, 4096, out);
    };
    auto run_graph = [&](const char* name, auto body) {
        hipGraph_t g; hipGraphExec_t ge;
        CK(hipStreamBeginCapture(s, hipStreamCaptureModeThreadLocal));
        body();
        CK(hipStreamEndCapture(s, &g));
        size_t nodes = 0; CK(hipGraphGetNodes(g, nullptr, &nodes));
        CK(hipGraphInstantiate(&ge, g, nullptr, nullptr, 0));
        const float t = timed([&] { CK(hipGraphLaunch(ge, s)); }, 5);
        // the host's share: how long the hipGraphLaunch call itself blocks the calling thread
        CK(hipStreamSynchronize(s));
        const auto h0 = std::chrono::steady_clock::now();
        CK(hipGraphLaunch(ge, s));
        const double host_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - h0).count();
        CK(hipStreamSynchronize(s));
        printf("  %-58s %.3f ms per window (%zu nodes); hipGraphLaunch blocks the host %.3f ms\n", name, t, nodes, host_ms);
        CK(hipGraphExecDestroy(ge)); CK(hipGraphDestroy(g));
    };
    run_graph("one chain, 27 x 39 kernels (side work inline):", [&] { for (int l = 0; l < L; ++l) for (int k = 0; k < per_layer; ++k) main_kernel(k, s); });
    run_graph("27 x [fork: 7 x 2-MB side | 32 main, join]:", [&] {
        for (int l = 0; l < L; ++l) {
            CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
            for (int k = 0; k < side; ++k) main_kernel(0, s2);
            for (int k = 0; k < per_layer - side; ++k) main_kernel(k, s);
            CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
        }
    });
    run_graph("27 x [fork: 7 side | 20 main, join, 12 main]:", [&] {
        for (int l = 0; l < L; ++l) {
            CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
            for (int k = 0; k < side; ++k) main_kernel(0, s2);
            for (int k = 0; k < 20; ++k) main_kernel(k, s);
            CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
            for (int k = 0; k < 12; ++k) main_kernel(k, s);
        }
    });
    run_graph("one chain, 2-MB kernels only (27 x 39):", [&] { for (int l = 0; l < L; ++l) for (int k = 0; k < per_layer; ++k) main_kernel(0, s); });
    run_graph("27 x [fork 7 | 32, join], 2-MB kernels only:", [&] {
        for (int l = 0; l < L; ++l) {
            CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
            for (int k = 0; k < side; ++k) main_kernel(0, s2);
            for (int k = 0; k < per_layer - side; ++k) main_kernel(0, s);
            CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
        }
    });
    return 0;
}
