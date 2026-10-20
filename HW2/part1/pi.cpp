#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <random>
#include <vector>
#include <time.h>
#include <immintrin.h>
#include <cstdint>
#define LL long long

std::vector<LL> thread_counts;
std::vector<LL> thread_N;

void* number_in_circle_thread(void* arg){
    long  thread_id = (long)arg;
    const LL n = thread_N[thread_id];
    // local one
    LL local_count = 0;
    alignas(64) uint32_t random_n[16];
    for(int i = 0 ; i < 16; i++){
        random_n[i] = rand();
    }
    __m512i state = _mm512_load_si512(random_n);
    const __m512i mask31 = _mm512_set1_epi32(0x7fffffff);
    const __m512 scale = _mm512_set1_ps(1.0f / 1073741824.0f);
    const __m512 one = _mm512_set1_ps(1.0f);
    for(LL i = 0; i < n; i += 16){
        // generate random numbers
        state = _mm512_xor_si512(state, _mm512_slli_epi32(state, 13));
        state = _mm512_xor_si512(state, _mm512_srli_epi32(state, 17));
        state = _mm512_xor_si512(state, _mm512_slli_epi32(state, 5));

        // load
        __m512 x_vec = _mm512_cvtepi32_ps(_mm512_and_si512(state, mask31));
        
        state = _mm512_xor_si512(state, _mm512_slli_epi32(state, 13));
        state = _mm512_xor_si512(state, _mm512_srli_epi32(state, 17));
        state = _mm512_xor_si512(state, _mm512_slli_epi32(state, 5));

        __m512 y_vec = _mm512_cvtepi32_ps(_mm512_and_si512(state, mask31));
        // mapping
        // Inputs are 31-bit values in [0, 2^31); scale to [-1, 1).
        __m512 x_mapped = _mm512_mul_ps(x_vec, scale);
        __m512 y_mapped = _mm512_mul_ps(y_vec, scale);
        x_vec = _mm512_sub_ps(x_mapped, one);
        y_vec = _mm512_sub_ps(y_mapped, one);

        // calculate
        __m512 x2 = _mm512_mul_ps(x_vec, x_vec);
        __m512 y2 = _mm512_mul_ps(y_vec, y_vec);
        __m512 sum = _mm512_add_ps(x2, y2);
        __mmask16 inside = _mm512_cmp_ps_mask(sum, one, _CMP_LE_OQ);
        const int remaining = static_cast<int>(n - i);
        const __mmask16 active = remaining >= 16 ? static_cast<__mmask16>(0xffff)
                                                 : static_cast<__mmask16>((1u << remaining) - 1u);
        local_count += __builtin_popcount(static_cast<unsigned int>(inside & active));

    }
    thread_counts[thread_id] = local_count;
    return NULL;
}

int main(int argc, char *argv[]){
    const int threads = argc > 1 ? atoi(argv[1]) : 1;
    const LL N = argc > 2 ? atoll(argv[2]) : 10000000;
    LL number_in_circle = 0;
    srand(time(NULL));
    // need optimize
    // for(LL i = 0; i < N; i++){
    //     double x = (double)rand() / RAND_MAX;
    //     double y = (double)rand() / RAND_MAX;
    //     if(x*x + y*y <= 1){
    //         number_in_circle++;
    //     }
    // }
    // optimize 
    std::vector<pthread_t> thread_pool(threads);
    thread_counts = std::vector<LL>(threads, 0);
    thread_N = std::vector<LL>(threads, N/threads);
    for(int i = 0 ; i < N%threads; i++){
        thread_N[i]++;
    }
    long thread_num;
    for(thread_num = 0; thread_num < threads; thread_num++){
        pthread_create(&thread_pool[thread_num], NULL, number_in_circle_thread, (void*)thread_num);
    }
    for(int i = 0 ; i < threads; i++){
        pthread_join(thread_pool[i], NULL);
    }
    for(int i = 0 ; i < threads; i++){
        number_in_circle += thread_counts[i];
    }
    double pi_estimate = (4.0 * number_in_circle) / N;
    printf("%lf\n",pi_estimate);
    return 0;
}
