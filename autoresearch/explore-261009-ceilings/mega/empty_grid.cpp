// An empty copy job (its count is 0 on device) at several grid sizes, in a graph chain like the window's:
// fetch_blobs launches 384 x 256 every layer and the PCIe share is nearly always empty.
// hipcc -O2 --offload-arch=gfx1100 empty_grid.cpp -o empty_grid && ./empty_grid 1
#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); exit(1); } } while (0)
__global__ void fetch_like(const unsigned long long* __restrict__ src, const int* __restrict__ n, uint4* __restrict__ dst, long long per) {
    const long long total = (long long) *n * per;
    for (long long i = (long long) blockIdx.x * blockDim.x + threadIdx.x; i < total; i += (long long) gridDim.x * blockDim.x) {
        const long long k = i / per, off = i - k * per;
        dst[i] = ((const uint4*) src[k])[off];
    }
}
int main(int argc, char** argv) {
    CK(hipSetDevice(argc > 1 ? atoi(argv[1]) : 0));
    int* n; unsigned long long* src; uint4* dst;
    CK(hipMalloc(&n, 4)); CK(hipMemset(n, 0, 4)); CK(hipMalloc(&src, 64)); CK(hipMalloc(&dst, 1 << 20));
    hipStream_t s; CK(hipStreamCreate(&s));
    hipEvent_t a, b; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
    const int N = 400;
    for (int blocks : {384, 352, 320, 256, 192, 96, 48, 16, 1}) {
        hipGraph_t g; hipGraphExec_t ge;
        CK(hipStreamBeginCapture(s, hipStreamCaptureModeThreadLocal));
        for (int i = 0; i < N; ++i) fetch_like<<<blocks, 256, 0, s>>>(src, n, dst, 147456);
        CK(hipStreamEndCapture(s, &g)); CK(hipGraphInstantiate(&ge, g, nullptr, nullptr, 0));
        CK(hipGraphLaunch(ge, s)); CK(hipStreamSynchronize(s));
        float best = 1e9;
        for (int r = 0; r < 5; ++r) {
            CK(hipEventRecord(a, s)); CK(hipGraphLaunch(ge, s)); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b));
            float ms; CK(hipEventElapsedTime(&ms, a, b)); if (ms < best) best = ms;
        }
        printf("  empty fetch, %3d blocks x 256: %.2f us per job in a graph chain\n", blocks, best * 1000 / N);
        CK(hipGraphExecDestroy(ge)); CK(hipGraphDestroy(g));
    }
    // a real fetch: one 2.3 MB blob from pinned, mapped host memory (the PCIe share's case) at each grid size
    const long long blob = 2441216;   // the largest expert blob, bytes (a multiple of 16)
    void* h; CK(hipHostMalloc(&h, blob, hipHostMallocMapped)); memset(h, 1, blob);
    void* hd; CK(hipHostGetDevicePointer(&hd, h, 0));
    unsigned long long p = (unsigned long long) hd; CK(hipMemcpy(src, &p, 8, hipMemcpyHostToDevice));
    int one = 1; CK(hipMemcpy(n, &one, 4, hipMemcpyHostToDevice));
    uint4* big; CK(hipMalloc(&big, blob));
    for (int blocks : {384, 192, 96, 48, 16}) {
        fetch_like<<<blocks, 256, 0, s>>>(src, n, big, blob / 16); CK(hipStreamSynchronize(s));
        float best = 1e9;
        for (int r = 0; r < 10; ++r) {
            CK(hipEventRecord(a, s)); fetch_like<<<blocks, 256, 0, s>>>(src, n, big, blob / 16); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b));
            float ms; CK(hipEventElapsedTime(&ms, a, b)); if (ms < best) best = ms;
        }
        printf("  one 2.3 MB blob from mapped host memory, %3d blocks: %.1f us (%.2f GB/s)\n", blocks, best * 1000, blob / best / 1e6);
    }
    return 0;
}
