// The per-layer fork/join pattern inside a hipGraph on gfx1100: 27 layers, each [fork: 7 kernels on the side stream |
// 6 kernels on the main stream, join], against the same 13 kernels as one chain. 2-MB reads each (the shared expert's
// and the router's size class).  hipcc -O2 --offload-arch=gfx1100 fork_cost.cpp -o fork_cost && ./fork_cost 1
#include <hip/hip_runtime.h>
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
    const int L = 27, side = 7, main_n = 6, N = L * (side + main_n);
    const size_t bytes = 2 << 20, n_vec = bytes / 16;
    uint4* buf; unsigned* out;
    CK(hipMalloc(&buf, bytes * N)); CK(hipMemset(buf, 1, bytes * N)); CK(hipMalloc(&out, 4));
    hipStream_t s, s2; CK(hipStreamCreate(&s)); CK(hipStreamCreate(&s2));
    hipEvent_t a, b, fork, join; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
    CK(hipEventCreateWithFlags(&fork, hipEventDisableTiming)); CK(hipEventCreateWithFlags(&join, hipEventDisableTiming));
    float ms;
    auto timed = [&](auto f, int reps) { f(); CK(hipStreamSynchronize(s)); CK(hipEventRecord(a, s)); for (int i = 0; i < reps; ++i) f(); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b)); CK(hipEventElapsedTime(&ms, a, b)); return ms / reps; };
    auto run_graph = [&](const char* name, auto body) {
        hipGraph_t g; hipGraphExec_t ge;
        CK(hipStreamBeginCapture(s, hipStreamCaptureModeThreadLocal));
        body();
        CK(hipStreamEndCapture(s, &g));
        size_t nodes = 0; CK(hipGraphGetNodes(g, nullptr, &nodes));
        CK(hipGraphInstantiate(&ge, g, nullptr, nullptr, 0));
        const float t = timed([&] { CK(hipGraphLaunch(ge, s)); }, 5);
        printf("  %-46s %.3f ms per window, %.2f us per kernel (%zu nodes)\n", name, t, t * 1000 / N, nodes);
        CK(hipGraphExecDestroy(ge)); CK(hipGraphDestroy(g));
    };
    for (int blocks : {192, 96}) {
        printf("== %d blocks x 256 per kernel\n", blocks);
        run_graph("one chain, 27 x 13 kernels:", [&] { for (int i = 0; i < N; ++i) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) i * n_vec, n_vec, out); });
        run_graph("27 x [fork 7 | 6, join]:", [&] {
            int i = 0;
            for (int l = 0; l < L; ++l) {
                CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
                for (int k = 0; k < side; ++k) slice_kernel<<<blocks, 256, 0, s2>>>(buf + (size_t) (i++) * n_vec, n_vec, out);
                for (int k = 0; k < main_n; ++k) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) (i++) * n_vec, n_vec, out);
                CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
            }
        });
        run_graph("27 x [fork 7 | 6, join], side kernels 1 block:", [&] {
            int i = 0;
            for (int l = 0; l < L; ++l) {
                CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
                for (int k = 0; k < side; ++k) slice_kernel<<<1, 256, 0, s2>>>(buf + (size_t) (i++) * n_vec, 4096, out);
                for (int k = 0; k < main_n; ++k) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) (i++) * n_vec, n_vec, out);
                CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
            }
        });
        run_graph("one chain, side kernels 1 block (the same work):", [&] {
            int i = 0;
            for (int l = 0; l < L; ++l) {
                for (int k = 0; k < side; ++k) slice_kernel<<<1, 256, 0, s>>>(buf + (size_t) (i++) * n_vec, 4096, out);
                for (int k = 0; k < main_n; ++k) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) (i++) * n_vec, n_vec, out);
            }
        });
    }
    return 0;
}
