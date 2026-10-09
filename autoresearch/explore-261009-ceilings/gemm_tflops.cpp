// Matrix compute ceiling per card: hipBLAS BF16 GEMM at prompt-path shapes (M tokens x K = 2560 by N = 6144 / 10240)
// and fp32 accumulate, as the prompt path's dense projections run (hipBLASLt / rocBLAS kernels).
// hipcc -O2 --offload-arch=gfx1100 gemm_tflops.cpp -o gemm_tflops -lhipblas && ./gemm_tflops
#include <hip/hip_runtime.h>
#include <hipblas/hipblas.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CK(x) do { auto e = (x); if ((int) e != 0) { printf("%s failed (%d)\n", #x, (int) e); exit(1); } } while (0)

int main() {
    int n = 0;
    CK(hipGetDeviceCount(&n));
    for (int dev = 0; dev < n; ++dev) {
        CK(hipSetDevice(dev));
        hipblasHandle_t h; CK(hipblasCreate(&h));
        hipEvent_t a, b; CK(hipEventCreate(&a)); CK(hipEventCreate(&b));
        printf("== device %d\n", dev);
        for (int M : {512, 2048, 8192}) for (int N : {6144, 10240}) {
            const int K = 2560;
            void *A, *B; float* C;
            CK(hipMalloc(&A, (size_t) M * K * 2)); CK(hipMalloc(&B, (size_t) K * N * 2)); CK(hipMalloc(&C, (size_t) M * N * 4));
            CK(hipMemset(A, 0x3c, (size_t) M * K * 2)); CK(hipMemset(B, 0x3c, (size_t) K * N * 2));
            const float one = 1.0f, zero = 0.0f;
            auto gemm = [&] {
                return hipblasGemmEx(h, HIPBLAS_OP_T, HIPBLAS_OP_N, N, M, K, &one, B, HIPBLAS_R_16B, K, A, HIPBLAS_R_16B, K,
                                     &zero, C, HIPBLAS_R_32F, N, HIPBLAS_COMPUTE_32F, HIPBLAS_GEMM_DEFAULT);
            };
            CK(gemm()); CK(hipDeviceSynchronize());
            const int reps = 20;
            CK(hipEventRecord(a, 0));
            for (int i = 0; i < reps; ++i) CK(gemm());
            CK(hipEventRecord(b, 0)); CK(hipEventSynchronize(b));
            float ms = 0; CK(hipEventElapsedTime(&ms, a, b)); ms /= reps;
            printf("  BF16 GEMM M=%5d K=%d N=%5d: %7.3f ms, %6.1f TFLOPS\n", M, K, N, ms, 2.0 * M * N * K / ms / 1e9);
            CK(hipFree(A)); CK(hipFree(B)); CK(hipFree(C));
        }
        CK(hipblasDestroy(h));
    }
    return 0;
}
