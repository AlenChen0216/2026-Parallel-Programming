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
    uint32_t __attribute__((aligned(32))) random_n[8];
    for(int i = 0 ; i < 8; i++){
        random_n[i] = rand();
    }
    __m256i_u state = _mm256_loadu_si256((__m256i_u*)random_n);
    for(LL i = 0; i < n; i+=8){
        // generate random numbers
        state = _mm256_xor_si256(state, _mm256_slli_epi32(state, 13));
        state = _mm256_xor_si256(state, _mm256_srli_epi32(state, 17));
        state = _mm256_xor_si256(state, _mm256_slli_epi32(state, 5));

        // load
        __m256 x_vec =  _mm256_cvtepi32_ps(_mm256_and_si256(state, _mm256_set1_epi32(0x7FFFFFFF)));  // _mm256_load_ps(x);
        
        state = _mm256_xor_si256(state, _mm256_slli_epi32(state, 13));
        state = _mm256_xor_si256(state, _mm256_srli_epi32(state, 17));
        state = _mm256_xor_si256(state, _mm256_slli_epi32(state, 5));

        __m256 y_vec =   _mm256_cvtepi32_ps(_mm256_and_si256(state, _mm256_set1_epi32(0x7FFFFFFF)));  // _mm256_load_ps(y);
        // mapping
        // Inputs are 31-bit values in [0, 2^31); scale to [-1, 1).
        __m256 scale = _mm256_set1_ps(1.0f / 1073741824.0f);
        __m256 x_mapped = _mm256_mul_ps(x_vec, scale);
        __m256 y_mapped = _mm256_mul_ps(y_vec, scale);
        x_vec = _mm256_sub_ps(x_mapped, _mm256_set1_ps(1.0f));
        y_vec = _mm256_sub_ps(y_mapped, _mm256_set1_ps(1.0f));

        // calculate
        __m256 x2 = _mm256_mul_ps(x_vec, x_vec);
        __m256 y2 = _mm256_mul_ps(y_vec, y_vec);
        __m256 sum = _mm256_add_ps(x2, y2);
        __m256 cmp = _mm256_cmp_ps(_mm256_set1_ps(1.0f), sum, _CMP_GE_OQ);
        int mask = _mm256_movemask_ps(cmp);
        local_count += __builtin_popcount(mask);

    }
    thread_counts[thread_id] = local_count;
    return NULL;
}

int main(int argc, char *argv[]){
    static const int threads = argv[1] ? atoi(argv[1]) : 1;
    static const LL N = argv[2] ? atoll(argv[2]) : 10000000;
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
