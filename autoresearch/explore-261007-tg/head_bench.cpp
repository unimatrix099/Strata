// the LM head's MMVQ alone (Q5_K, 2560 x 248320, the model's output.weight shape): time and bandwidth per column count
#include "strata/kernels/native_mmvq.hpp"
#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
static unsigned lcg() { static unsigned long long z = 42; z = z * 6364136223846793005ull + 1442695040888963407ull; return unsigned(z >> 33); }
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int n_in = 2560, n_out = 248320, type = 13;
    const size_t wb = strata::kernels::native_mmvq_weight_bytes(type, n_in, n_out);
    void *w, *x; float* y;
    hipMalloc(&w, wb); hipMalloc(&x, 8 * (n_in / 32) * 36); hipMalloc(&y, (size_t) 8 * n_out * 4);
    std::vector<unsigned char> h(wb);
    for (size_t i = 0; i < wb; ++i) h[i] = (unsigned char) (lcg() & 0x3f);   // small values: finite scales
    hipMemcpy(w, h.data(), wb, hipMemcpyHostToDevice);
    std::vector<unsigned char> hx(8 * (n_in / 32) * 36); for (auto& v : hx) v = (unsigned char) (lcg() & 0x1f);
    hipMemcpy(x, hx.data(), hx.size(), hipMemcpyHostToDevice);
    hipStream_t s; hipStreamCreate(&s);
    hipEvent_t a, b; hipEventCreate(&a); hipEventCreate(&b);
    printf("weights %.1f MB\n", wb / 1e6);
    for (int nc = 1; nc <= 4; ++nc) {
        for (int i = 0; i < 5; ++i) strata::kernels::native_mmvq(type, w, x, y, n_in, n_out, nc, s);
        const int reps = 50;
        hipEventRecord(a, s);
        for (int i = 0; i < reps; ++i) strata::kernels::native_mmvq(type, w, x, y, n_in, n_out, nc, s);
        hipEventRecord(b, s); hipEventSynchronize(b);
        float ms = 0; hipEventElapsedTime(&ms, a, b); ms /= reps;
        std::vector<float> ym((size_t) nc * n_out), y1(n_out);
        hipStreamSynchronize(s); hipMemcpy(ym.data(), y, ym.size() * 4, hipMemcpyDeviceToHost);
        int bad = 0; unsigned long long hsum = 1469598103934665603ull;
        for (int j = 0; j < nc; ++j) {
            strata::kernels::native_mmvq(type, w, (char*) x + (size_t) j * (n_in / 32) * 36, y, n_in, n_out, 1, s);
            hipStreamSynchronize(s); hipMemcpy(y1.data(), y, n_out * 4, hipMemcpyDeviceToHost);
            if (memcmp(y1.data(), ym.data() + (size_t) j * n_out, n_out * 4)) ++bad;
        }
        { int nan = 0, diff0 = -1; for (size_t q = 0; q < ym.size(); ++q) if (ym[q] != ym[q]) ++nan;
          printf("  nan %d first %.6g %.6g %.6g\n", nan, ym[0], ym[1], ym[n_out-1]); (void) diff0; }
        for (float v : ym) { unsigned u; memcpy(&u, &v, 4); hsum = (hsum ^ u) * 1099511628211ull; }
        printf("%d column(s): %.3f ms, %.0f GB/s of weights, columns unequal to 1-col: %d, hash %016llx\n", nc, ms,
               wb / (ms * 1e6), bad, hsum);
    }
    return 0;
}
