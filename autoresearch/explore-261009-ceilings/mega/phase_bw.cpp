// VRAM-bound phases: N kernels in a graph against one persistent kernel with an atomic grid barrier, each phase reading
// a fresh slice of a 1.5 GB buffer (no cache reuse) of 2 / 8 / 24 / 96 MB - expert-blob-sized to projection-sized -
// at 96 / 192 / 384 blocks. The question: does a fixed persistent grid stream VRAM as well as per-kernel grids, and
// what does a phase of 2 MB cost each way.  hipcc -O2 --offload-arch=gfx1100 phase_bw.cpp -o phase_bw && ./phase_bw 1
#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); exit(1); } } while (0)

__device__ __forceinline__ unsigned fold_slice(const uint4* p, size_t n_vec) {
    unsigned acc = 0;
    for (size_t i = blockIdx.x * (size_t) blockDim.x + threadIdx.x; i < n_vec; i += (size_t) gridDim.x * blockDim.x) {
        const uint4 v = p[i]; acc ^= v.x ^ v.y ^ v.z ^ v.w;
    }
    return acc;
}
__global__ void slice_kernel(const uint4* p, size_t n_vec, unsigned* out) {
    const unsigned a = fold_slice(p, n_vec);
    if (a == 0x12345678u) *out = a;
}
__device__ __forceinline__ void grid_barrier(unsigned* bar, unsigned nblocks, unsigned& gen) {
    __syncthreads();
    if (threadIdx.x == 0) {
        gen += nblocks; __threadfence(); atomicAdd(bar, 1u);
        while (atomicAdd(bar, 0u) < gen) { __builtin_amdgcn_s_sleep(1); }
        __threadfence();
    }
    __syncthreads();
}
__global__ void persistent_kernel(const uint4* base, size_t n_vec, int phases, unsigned* bar, unsigned* out) {
    unsigned gen = 0, acc = 0;
    for (int ph = 0; ph < phases; ++ph) {
        acc ^= fold_slice(base + (size_t) ph * n_vec, n_vec);
        grid_barrier(bar, gridDim.x, gen);
    }
    if (acc == 0x12345678u) *out = acc;
}

int main(int argc, char** argv) {
    const int dev = argc > 1 ? atoi(argv[1]) : 0;
    CK(hipSetDevice(dev));
    const size_t total = (size_t) (argc > 2 ? atoi(argv[2]) : 384) << 20;   // MB; above the 96 MB infinity cache
    uint4* buf; unsigned *out, *bar;
    CK(hipMalloc(&buf, total)); CK(hipMemset(buf, 1, total));
    CK(hipMalloc(&out, 4)); CK(hipMalloc(&bar, 4));
    hipStream_t s; CK(hipStreamCreate(&s));
    hipEvent_t a, b; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
    float ms;
    auto timed = [&](auto f, int reps) { f(); CK(hipStreamSynchronize(s)); CK(hipEventRecord(a, s)); for (int i = 0; i < reps; ++i) f(); CK(hipEventRecord(b, s)); CK(hipEventSynchronize(b)); CK(hipEventElapsedTime(&ms, a, b)); return ms / reps; };
    setvbuf(stdout, nullptr, _IONBF, 0);
    hipDeviceProp_t pr; CK(hipGetDeviceProperties(&pr, dev));
    int per_cu = 0;
    CK(hipOccupancyMaxActiveBlocksPerMultiprocessor(&per_cu, persistent_kernel, 256, 0));
    const int max_resident = per_cu * pr.multiProcessorCount;   // a persistent grid above this deadlocks at its barrier
    printf("== device %d: %d multiprocessors x %d resident blocks of 256 = %d blocks at most for a persistent kernel\n",
           dev, pr.multiProcessorCount, per_cu, max_resident);
    printf("   per phase: graph of kernels | persistent (atomic barrier); GB/s of each\n");
    for (size_t mb : {2, 8, 24, 96}) {
        const size_t bytes = mb << 20, n_vec = bytes / 16;
        const int N = (int) (total / bytes);
        for (int blocks : {96, 192, 384, 768}) {
            if (blocks > max_resident) { printf("  %3zu MB phases, %4d blocks: above the resident limit, skipped\n", mb, blocks); continue; }
            hipGraph_t g; hipGraphExec_t ge;
            CK(hipStreamBeginCapture(s, hipStreamCaptureModeThreadLocal));
            for (int i = 0; i < N; ++i) slice_kernel<<<blocks, 256, 0, s>>>(buf + (size_t) i * n_vec, n_vec, out);
            CK(hipStreamEndCapture(s, &g));
            CK(hipGraphInstantiate(&ge, g, nullptr, nullptr, 0));
            const float tg = timed([&] { CK(hipGraphLaunch(ge, s)); }, 3) / N;
            const float tp = timed([&] { CK(hipMemsetAsync(bar, 0, 4, s)); persistent_kernel<<<blocks, 256, 0, s>>>(buf, n_vec, N, bar, out); }, 3) / N;
            printf("  %3zu MB phases, %4d blocks: graph %7.2f us (%6.0f GB/s) | persistent %7.2f us (%6.0f GB/s)  -> %.1f%% less\n",
                   mb, blocks, tg * 1000, bytes / tg / 1e6, tp * 1000, bytes / tp / 1e6, 100 * (1 - tp / tg));
            CK(hipGraphExecDestroy(ge)); CK(hipGraphDestroy(g));
        }
    }
    return 0;
}
