// build: hipcc -O2 --offload-arch=gfx1100 p2p_pingpong.hip -o p2p_pingpong; run: HIP_VISIBLE_DEVICES=1,0 ./p2p_pingpong <floats>
// one-way GPU->GPU latency for a small P2P write + flag (the cost of one tensor-parallel exchange)
#include <hip/hip_runtime.h>
#include <cstdio>
#include <chrono>
#define CK(x) do { hipError_t e = (x); if (e != hipSuccess) { printf("%s: %s\n", #x, hipGetErrorString(e)); return 1; } } while (0)
__global__ void player(float* peer_buf, volatile unsigned* peer_flag, volatile unsigned* my_flag, int n_floats,
                       int iters, int first) {
    for (int i = 0; i < iters; ++i) {
        const unsigned want = 2u * i + (first ? 0u : 1u);   // whose turn: A sends even, B odd
        if (!(first && i == 0)) {
            if (threadIdx.x == 0) while (*my_flag < want) {}
            __syncthreads();
        }
        for (int j = threadIdx.x; j < n_floats; j += blockDim.x) peer_buf[j] = (float) (i + j);
        __threadfence_system();
        __syncthreads();
        if (threadIdx.x == 0) *peer_flag = want + 1;
    }
}
int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int n_floats = argc > 1 ? atoi(argv[1]) : 16384, iters = 10000;
    int can01 = 0, can10 = 0;
    CK(hipDeviceCanAccessPeer(&can01, 0, 1)); CK(hipDeviceCanAccessPeer(&can10, 1, 0));
    printf("P2P access 0->1 %d, 1->0 %d\n", can01, can10);
    float* buf[2]; unsigned* flag[2];
    for (int d = 0; d < 2; ++d) {
        CK(hipSetDevice(d)); CK(hipDeviceEnablePeerAccess(1 - d, 0));
        CK(hipExtMallocWithFlags((void**) &buf[d], n_floats * 4, hipDeviceMallocUncached)); CK(hipExtMallocWithFlags((void**) &flag[d], 64, hipDeviceMallocUncached)); CK(hipMemset(flag[d], 0, 64));
        CK(hipDeviceSynchronize());
    }
    hipStream_t s[2];
    for (int d = 0; d < 2; ++d) { CK(hipSetDevice(d)); CK(hipStreamCreateWithFlags(&s[d], hipStreamNonBlocking)); }
    auto t0 = std::chrono::steady_clock::now();
    CK(hipSetDevice(1)); hipLaunchKernelGGL(player, 1, 256, 0, s[1], buf[0], flag[0], flag[1], n_floats, iters, 0);
    CK(hipSetDevice(0)); hipLaunchKernelGGL(player, 1, 256, 0, s[0], buf[1], flag[1], flag[0], n_floats, iters, 1);
    for (int d = 0; d < 2; ++d) { CK(hipSetDevice(d)); CK(hipStreamSynchronize(s[d])); }
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    printf("payload %d bytes: %.2f us per one-way exchange (%d round trips, %.1f ms)\n", n_floats * 4, us / (2.0 * iters),
           iters, us / 1000);
    return 0;
}
