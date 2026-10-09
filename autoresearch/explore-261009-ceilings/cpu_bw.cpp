// CPU RAM read bandwidth on this PC with 1..N threads (the ceiling of the CPU's expert rows and of pinned-RAM reads).
// g++ -O3 -march=native -fopenmp cpu_bw.cpp -o cpu_bw && ./cpu_bw
#include <chrono>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <omp.h>
#include <immintrin.h>

int main() {
    const size_t bytes = (size_t) 4 << 30;   // 4 GiB: far beyond the caches
    char* p = (char*) aligned_alloc(64, bytes);
    #pragma omp parallel for
    for (size_t i = 0; i < bytes; i += 4096) memset(p + i, (int) (i >> 12), 4096);
    const int maxt = omp_get_max_threads();
    for (int t : {1, 2, 4, 8, 16, maxt}) {
        if (t > maxt) continue;
        omp_set_num_threads(t);
        double best = 1e9;
        for (int rep = 0; rep < 3; ++rep) {
            const auto t0 = std::chrono::steady_clock::now();
            __m256i acc_all = _mm256_setzero_si256();
            #pragma omp parallel
            {
                __m256i acc = _mm256_setzero_si256();
                #pragma omp for schedule(static)
                for (size_t i = 0; i < bytes; i += 256) {
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 32)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 64)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 96)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 128)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 160)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 192)));
                    acc = _mm256_xor_si256(acc, _mm256_load_si256((const __m256i*) (p + i + 224)));
                }
                #pragma omp critical
                acc_all = _mm256_xor_si256(acc_all, acc);
            }
            const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            if (s < best) best = s;
            if (_mm256_extract_epi32(acc_all, 0) == 0x12345678) printf("x");
        }
        printf("%2d threads: %6.1f GB/s read\n", t, bytes / best / 1e9);
    }
    return 0;
}
