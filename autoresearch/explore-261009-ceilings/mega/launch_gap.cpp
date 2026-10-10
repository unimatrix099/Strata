// What a kernel launch costs on this card against a device-side grid barrier, for the "persistent layer kernel" question.
// (1) N empty kernels in a hipGraph: the per-launch gap inside a graph (what each small kernel pays at least);
// (2) N small kernels (each block reads 256 KB) in a graph;
// (3) one persistent kernel doing the same N phases with a grid-wide barrier (atomic counter, all blocks resident);
// (4) the same with hipLaunchCooperativeKernel + cooperative_groups grid sync, if the runtime allows it.
// hipcc -O2 --offload-arch=gfx1100 launch_gap.cpp -o launch_gap && ./launch_gap
#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
#include <cstdio>
#include <cstdlib>
namespace cg = cooperative_groups;
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); exit(1); } } while (0)

__global__ void empty_kernel(unsigned* out) { if (threadIdx.x == 9999) *out = 1; }

// every block reads `per_block` bytes (grid-strided) and folds them into one value
__device__ __forceinline__ unsigned fold(const uint4* p, size_t n_vec) {
    unsigned acc = 0;
    for (size_t i = threadIdx.x; i < n_vec; i += blockDim.x) { const uint4 v = p[i]; acc ^= v.x ^ v.y ^ v.z ^ v.w; }
    return acc;
}
__global__ void small_kernel(const uint4* p, size_t n_vec_per_block, unsigned* out) {
    const unsigned a = fold(p + blockIdx.x * n_vec_per_block, n_vec_per_block);
    if (a == 0x12345678u) *out = a;
}
// a software grid barrier: a monotonically increasing counter; block 0's thread 0 is not special
__device__ __forceinline__ void grid_barrier(unsigned* bar, unsigned nblocks, unsigned& gen) {
    __syncthreads();
    if (threadIdx.x == 0) {
        gen += nblocks;
        __threadfence();
        atomicAdd(bar, 1u);
        while (atomicAdd(bar, 0u) < gen) { __builtin_amdgcn_s_sleep(1); }
        __threadfence();
    }
    __syncthreads();
}
__global__ void persistent_kernel(const uint4* p, size_t n_vec_per_block, int phases, unsigned* bar, unsigned* out) {
    unsigned gen = 0, acc = 0;
    for (int ph = 0; ph < phases; ++ph) {
        acc ^= fold(p + blockIdx.x * n_vec_per_block, n_vec_per_block);
        grid_barrier(bar, gridDim.x, gen);
    }
    if (acc == 0x12345678u) *out = acc;
}
__global__ void coop_kernel(const uint4* p, size_t n_vec_per_block, int phases, unsigned* out) {
    cg::grid_group g = cg::this_grid();
    unsigned acc = 0;
    for (int ph = 0; ph < phases; ++ph) {
        acc ^= fold(p + blockIdx.x * n_vec_per_block, n_vec_per_block);
        g.sync();
    }
    if (acc == 0x12345678u) *out = acc;
}

int main(int argc, char** argv) {
    const int dev = argc > 1 ? atoi(argv[1]) : 0;
    CK(hipSetDevice(dev));
    hipDeviceProp_t pr; CK(hipGetDeviceProperties(&pr, dev));
    printf("== device %d: %s, %d CUs, cooperativeLaunch %d\n", dev, pr.name, pr.multiProcessorCount, pr.cooperativeLaunch);
    const int N = 500;                 // phases / kernels (a window has ~940 kernels per card)
    const int blocks = pr.multiProcessorCount * 2;   // 192: all resident at once
    const size_t per_block = 256 * 1024;             // bytes read per block per phase: 48 MB per phase over the grid
    const size_t n_vec = per_block / 16;
    uint4* buf; unsigned *out, *bar;
    CK(hipMalloc(&buf, per_block * blocks)); CK(hipMemset(buf, 1, per_block * blocks));
    CK(hipMalloc(&out, 4)); CK(hipMalloc(&bar, 4)); CK(hipMemset(bar, 0, 4));
    hipStream_t s; CK(hipStreamCreate(&s));
    hipEvent_t a, b; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
    float ms;
    auto timed = [&](auto f, int reps) { f(); CK(hipStreamSynchronize(s)); CK(hipEventRecord(a, s)); for (int i = 0; i < reps; ++i) f(); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b)); CK(hipEventElapsedTime(&ms, a, b)); return ms / reps; };

    // (1) and (2): graphs of N kernels
    for (int kind = 0; kind < 2; ++kind) {
        hipGraph_t g; hipGraphExec_t ge;
        CK(hipStreamBeginCapture(s, hipStreamCaptureModeThreadLocal));
        for (int i = 0; i < N; ++i) {
            if (kind == 0) empty_kernel<<<blocks, 256, 0, s>>>(out);
            else small_kernel<<<blocks, 256, 0, s>>>(buf, n_vec, out);
        }
        CK(hipStreamEndCapture(s, &g));
        CK(hipGraphInstantiate(&ge, g, nullptr, nullptr, 0));
        const float t = timed([&] { CK(hipGraphLaunch(ge, s)); }, 5);
        printf("  graph of %d %s kernels (%d blocks x 256): %.3f ms -> %.2f us per kernel\n", N,
               kind == 0 ? "empty" : "256KB-per-block", blocks, t, t * 1000 / N);
        // the same N launches as plain stream launches (no graph)
        const float t2 = timed([&] { for (int i = 0; i < N; ++i) { if (kind == 0) empty_kernel<<<blocks, 256, 0, s>>>(out); else small_kernel<<<blocks, 256, 0, s>>>(buf, n_vec, out); } }, 3);
        printf("  %d %s kernels as stream launches: %.3f ms -> %.2f us per kernel\n", N, kind == 0 ? "empty" : "256KB-per-block", t2, t2 * 1000 / N);
        CK(hipGraphExecDestroy(ge)); CK(hipGraphDestroy(g));
    }
    // (3) persistent with the atomic barrier
    {
        const float t = timed([&] { CK(hipMemsetAsync(bar, 0, 4, s)); persistent_kernel<<<blocks, 256, 0, s>>>(buf, n_vec, N, bar, out); }, 5);
        printf("  one persistent kernel, %d phases x 256KB-per-block, atomic grid barrier: %.3f ms -> %.2f us per phase\n", N, t, t * 1000 / N);
        const float t0 = timed([&] { CK(hipMemsetAsync(bar, 0, 4, s)); persistent_kernel<<<blocks, 256, 0, s>>>(buf, 0, N, bar, out); }, 5);
        printf("  one persistent kernel, %d empty phases, atomic grid barrier: %.3f ms -> %.2f us per barrier\n", N, t0, t0 * 1000 / N);
    }
    // (4) cooperative launch
    {
        int ph = N; size_t nv = n_vec; void* args[] = {&buf, &nv, &ph, &out};
        hipError_t e = hipLaunchCooperativeKernel((const void*) coop_kernel, dim3(blocks), dim3(256), args, 0, s);
        if (e != hipSuccess) printf("  cooperative launch: %s\n", hipGetErrorString(e));
        else {
            CK(hipStreamSynchronize(s));
            const float t = timed([&] { CK(hipLaunchCooperativeKernel((const void*) coop_kernel, dim3(blocks), dim3(256), args, 0, s)); }, 5);
            printf("  cooperative kernel, %d phases x 256KB-per-block, grid.sync(): %.3f ms -> %.2f us per phase\n", N, t, t * 1000 / N);
            nv = 0;
            const float t0 = timed([&] { CK(hipLaunchCooperativeKernel((const void*) coop_kernel, dim3(blocks), dim3(256), args, 0, s)); }, 5);
            printf("  cooperative kernel, %d empty phases, grid.sync(): %.3f ms -> %.2f us per sync\n", N, t0, t0 * 1000 / N);
        }
    }
    return 0;
}
