// build: hipcc -O2 --offload-arch=gfx1100 graph_gap.hip -o graph_gap; run: HIP_VISIBLE_DEVICES=1 ./graph_gap
// cost of one more kernel in a captured HIP graph on this GPU: replay graphs of N kernels and time them
#include <hip/hip_runtime.h>
#include <cstdio>
#include <chrono>
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); return 1; } } while (0)
__global__ void tiny(float* p, int n) { int i = blockIdx.x * blockDim.x + threadIdx.x; if (i < n) p[i] = p[i] * 1.0001f + 1.0f; }
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    float* d; CK(hipMalloc(&d, 1 << 22));
    hipStream_t s; CK(hipStreamCreateWithFlags(&s, hipStreamNonBlocking));
    for (int blocks : {1, 40}) {
        for (int N : {100, 1000, 2400}) {
            hipGraph_t g; hipGraphExec_t ge;
            CK(hipStreamBeginCapture(s, hipStreamCaptureModeThreadLocal));
            for (int k = 0; k < N; ++k) hipLaunchKernelGGL(tiny, blocks, 256, 0, s, d, blocks * 256);
            CK(hipStreamEndCapture(s, &g)); CK(hipGraphInstantiate(&ge, g, nullptr, nullptr, 0));
            for (int w = 0; w < 3; ++w) CK(hipGraphLaunch(ge, s));
            CK(hipStreamSynchronize(s));
            const int reps = 20;
            auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; ++r) CK(hipGraphLaunch(ge, s));
            CK(hipStreamSynchronize(s));
            double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / reps;
            printf("%2d blocks x 256 threads, %4d kernels: %8.1f us per graph = %.2f us per kernel\n", blocks, N, us, us / N);
            hipGraphExecDestroy(ge); hipGraphDestroy(g);
        }
    }
    return 0;
}
