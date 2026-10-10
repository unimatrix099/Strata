// Does the any-order overlap survive hipGraph capture, and do two parallel graph branches (fork/join on events) run
// side by side on this ROCm?  150 2-MB-read kernels each way.  hipcc -O2 --offload-arch=gfx1100 any_order_graph.cpp -o any_order_graph
#include <hip/hip_runtime.h>
#include <hip/hip_ext.h>
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
    const int N = 150, blocks = 192;
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
        printf("  %-52s %.2f us per kernel (%zu nodes)\n", name, t * 1000 / N, nodes);
        CK(hipGraphExecDestroy(ge)); CK(hipGraphDestroy(g));
    };
    run_graph("graph, plain launches:", [&] { for (int i = 0; i < N; ++i) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) i * n_vec, n_vec, out); });
    run_graph("graph, hipExtAnyOrderLaunch captured:", [&] { for (int i = 0; i < N; ++i) hipExtLaunchKernelGGL(slice_kernel, dim3(blocks), dim3(256), 0, s, nullptr, nullptr, hipExtAnyOrderLaunch, (const uint4*) (buf + (size_t) i * n_vec), n_vec, out); });
    run_graph("graph, two branches (fork/join), 75 kernels each:", [&] {
        CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
        for (int i = 0; i < N; ++i) slice_kernel<<<blocks, 256, 0, (i & 1) ? s2 : s>>>(buf + (size_t) i * n_vec, n_vec, out);
        CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
    });
    run_graph("graph, two branches, 96-block kernels:", [&] {
        CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
        for (int i = 0; i < N; ++i) slice_kernel<<<96, 256, 0, (i & 1) ? s2 : s>>>(buf + (size_t) i * n_vec, n_vec, out);
        CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0));
    });
    const float t = timed([&] {
        CK(hipEventRecord(fork, s)); CK(hipStreamWaitEvent(s2, fork, 0));
        for (int i = 0; i < N; ++i) slice_kernel<<<96, 256, 0, (i & 1) ? s2 : s>>>(buf + (size_t) i * n_vec, n_vec, out);
        CK(hipEventRecord(join, s2)); CK(hipStreamWaitEvent(s, join, 0)); }, 3);
    printf("  %-52s %.2f us per kernel\n", "no graph, two streams, 96-block kernels:", t * 1000 / N);
    return 0;
}
