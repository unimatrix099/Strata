// Does HIP's hipExtAnyOrderLaunch (no barrier packet between kernels of one stream) shrink the per-kernel gap on gfx1100?
// 500 empty kernels and 500 2-MB-read kernels: plain stream launches against hipExtLaunchKernelGGL with the flag.
// hipcc -O2 --offload-arch=gfx1100 any_order.cpp -o any_order && ./any_order 1
#include <hip/hip_runtime.h>
#include <hip/hip_ext.h>
#include <cstdio>
#include <cstdlib>
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); exit(1); } } while (0)
__global__ void empty_kernel(unsigned* out) { if (threadIdx.x == 9999) *out = 1; }
__global__ void slice_kernel(const uint4* p, size_t n_vec, unsigned* out) {
    unsigned acc = 0;
    for (size_t i = blockIdx.x * (size_t) blockDim.x + threadIdx.x; i < n_vec; i += (size_t) gridDim.x * blockDim.x) { const uint4 v = p[i]; acc ^= v.x ^ v.y ^ v.z ^ v.w; }
    if (acc == 0x12345678u) *out = acc;
}
int main(int argc, char** argv) {
    const int dev = argc > 1 ? atoi(argv[1]) : 0;
    CK(hipSetDevice(dev));
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int N = 150, blocks = 192;   // 300 MB: fits beside the server
    const size_t bytes = 2 << 20, n_vec = bytes / 16;
    uint4* buf; unsigned* out;
    CK(hipMalloc(&buf, bytes * N)); CK(hipMemset(buf, 1, bytes * N)); CK(hipMalloc(&out, 4));
    hipStream_t s; CK(hipStreamCreate(&s));
    hipEvent_t a, b; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
    float ms;
    auto timed = [&](auto f, int reps) { f(); CK(hipStreamSynchronize(s)); CK(hipEventRecord(a, s)); for (int i = 0; i < reps; ++i) f(); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b)); CK(hipEventElapsedTime(&ms, a, b)); return ms / reps; };
    float t;
    t = timed([&] { for (int i = 0; i < N; ++i) empty_kernel<<<blocks, 256, 0, s>>>(out); }, 3);
    printf("  %d empty kernels, stream launches:            %.2f us per kernel\n", N, t * 1000 / N);
    t = timed([&] { for (int i = 0; i < N; ++i) hipExtLaunchKernelGGL(empty_kernel, dim3(blocks), dim3(256), 0, s, nullptr, nullptr, hipExtAnyOrderLaunch, out); }, 3);
    printf("  %d empty kernels, hipExtAnyOrderLaunch:        %.2f us per kernel\n", N, t * 1000 / N);
    t = timed([&] { for (int i = 0; i < N; ++i) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) i * n_vec, n_vec, out); }, 3);
    printf("  %d 2-MB kernels, stream launches:             %.2f us per kernel (%.0f GB/s)\n", N, t * 1000 / N, bytes / (t / N) / 1e6);
    t = timed([&] { for (int i = 0; i < N; ++i) hipExtLaunchKernelGGL(slice_kernel, dim3(blocks), dim3(256), 0, s, nullptr, nullptr, hipExtAnyOrderLaunch, (const uint4*) (buf + (size_t) i * n_vec), n_vec, out); }, 3);
    printf("  %d 2-MB kernels, hipExtAnyOrderLaunch:         %.2f us per kernel (%.0f GB/s)\n", N, t * 1000 / N, bytes / (t / N) / 1e6);
    return 0;
}
