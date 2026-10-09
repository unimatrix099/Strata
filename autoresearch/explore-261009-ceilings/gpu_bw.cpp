// GPU ceilings on this PC: per card, the VRAM read bandwidth a streaming kernel reaches (the number decode is bound
// by), the hipMemcpy D2D bandwidth, pinned host->device and device->host over PCIe, and card-to-card peer copies.
// hipcc -O2 --offload-arch=gfx1100 gpu_bw.cpp -o gpu_bw && ./gpu_bw
#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); exit(1); } } while (0)

// every thread reads 16-byte vectors with a grid stride and folds them into one value (never optimized away)
__global__ void read_kernel(const uint4* __restrict__ p, size_t n, unsigned* out) {
    unsigned acc = 0;
    for (size_t i = blockIdx.x * (size_t) blockDim.x + threadIdx.x; i < n; i += (size_t) gridDim.x * blockDim.x) {
        const uint4 v = p[i];
        acc ^= v.x ^ v.y ^ v.z ^ v.w;
    }
    if (acc == 0x12345678u) *out = acc;
}

static float time_ms(hipStream_t s, hipEvent_t a, hipEvent_t b, int reps, void (*f)(void*), void* ctx) {
    f(ctx);   // warm
    CK(hipEventRecord(a, s));
    for (int i = 0; i < reps; ++i) f(ctx);
    CK(hipEventRecord(b, s));
    CK(hipEventSynchronize(b));
    float ms = 0;
    CK(hipEventElapsedTime(&ms, a, b));
    return ms / reps;
}

struct Ctx { hipStream_t s; void *src, *dst, *h; unsigned* out; size_t bytes; int blocks; hipMemcpyKind kind; };
static void k_read(void* c) { Ctx* x = (Ctx*) c; read_kernel<<<x->blocks, 256, 0, x->s>>>((const uint4*) x->src, x->bytes / 16, x->out); }
static void k_copy(void* c) { Ctx* x = (Ctx*) c; CK(hipMemcpyAsync(x->dst, x->src, x->bytes, x->kind, x->s)); }

int main() {
    int n = 0;
    CK(hipGetDeviceCount(&n));
    const size_t bytes = (size_t) 1 << 30;   // 1 GiB
    void* h = nullptr;
    CK(hipHostMalloc(&h, bytes, hipHostMallocDefault));
    std::vector<void*> d(n);
    for (int dev = 0; dev < n; ++dev) {
        CK(hipSetDevice(dev));
        hipDeviceProp_t pr;
        CK(hipGetDeviceProperties(&pr, dev));
        printf("== device %d: %s, %d CUs, %d MHz, mem %d MHz x %d-bit (theoretical %.0f GB/s), L2 %d KB\n", dev, pr.name,
               pr.multiProcessorCount, pr.clockRate / 1000, pr.memoryClockRate / 1000, pr.memoryBusWidth,
               2.0 * pr.memoryClockRate * 1e3 * pr.memoryBusWidth / 8 / 1e9, pr.l2CacheSize / 1024);
        hipStream_t s; hipEvent_t a, b;
        CK(hipStreamCreate(&s)); CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
        void *src, *dst; unsigned* out;
        CK(hipMalloc(&src, bytes)); CK(hipMalloc(&dst, bytes)); CK(hipMalloc(&out, 4));
        CK(hipMemset(src, 1, bytes));
        d[dev] = src;
        for (int blocks : {pr.multiProcessorCount * 4, pr.multiProcessorCount * 16, pr.multiProcessorCount * 64}) {
            Ctx c{s, src, dst, h, out, bytes, blocks, hipMemcpyDeviceToDevice};
            const float ms = time_ms(s, a, b, 10, k_read, &c);
            printf("  VRAM read kernel, %5d blocks x 256: %7.1f GB/s\n", blocks, bytes / ms / 1e6);
        }
        {
            Ctx c{s, src, dst, h, out, bytes, 0, hipMemcpyDeviceToDevice};
            printf("  hipMemcpy D2D (read+write):       %7.1f GB/s\n", 2 * bytes / time_ms(s, a, b, 10, k_copy, &c) / 1e6);
            Ctx c2{s, h, dst, h, out, bytes, 0, hipMemcpyHostToDevice};
            printf("  pinned host -> device (PCIe):     %7.2f GB/s\n", bytes / time_ms(s, a, b, 5, k_copy, &c2) / 1e6);
            Ctx c3{s, src, h, h, out, bytes, 0, hipMemcpyDeviceToHost};
            printf("  device -> pinned host (PCIe):     %7.2f GB/s\n", bytes / time_ms(s, a, b, 5, k_copy, &c3) / 1e6);
        }
        CK(hipFree(dst)); CK(hipFree(out));
    }
    if (n >= 2) {
        for (int from = 0; from < n; ++from) for (int to = 0; to < n; ++to) if (from != to) {
            CK(hipSetDevice(to));
            int can = 0; CK(hipDeviceCanAccessPeer(&can, to, from));
            hipStream_t s; hipEvent_t a, b;
            CK(hipStreamCreate(&s)); CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
            void* dst; CK(hipMalloc(&dst, bytes));
            Ctx c{s, d[from], dst, h, nullptr, bytes, 0, hipMemcpyDeviceToDevice};
            printf("== card %d -> card %d (hipMemcpy, peer access %s): %7.2f GB/s\n", from, to, can ? "yes" : "no",
                   bytes / time_ms(s, a, b, 5, k_copy, &c) / 1e6);
            CK(hipFree(dst));
        }
    }
    return 0;
}
