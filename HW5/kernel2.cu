#include <cstdio>
#include <cstdlib>
#include <cuda.h>
#include <cstring>
#include <chrono>
#include <algorithm>

#define N 16

__global__ void mandel_kernel(int *output,const  float step_x, const float step_y, const float x0, const float y0, const int count, const int width, const int height, const size_t pitch)
{
    // To avoid error caused by the floating number, use the following pseudo code
    //
    // float x = lowerX + thisX * stepX;
    // float y = lowerY + thisY * stepY;
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if (i >= width || j >= height)
        return;
    int padding = pitch / sizeof(int);
    float x = x0 + ((float)i * step_x);
    float y = y0 + ((float)j * step_y);
    int index = ((j * padding) + i);

    
    float zre = x, zim = y;
    float saved_re = zre, saved_im = zim;
    int res;
    for(res = 0 ; res < count; ++res){
        float re2 = zre * zre;
        float im2 = zim * zim;
        if(re2 + im2 > 4.f)
            break;
        float new_re = re2 - im2;
        float new_im = 2.f * zre * zim;
        zre = x + new_re;
        zim = y + new_im;
        if(__float_as_uint(zre) == __float_as_uint(saved_re) && __float_as_uint(zim) == __float_as_uint(saved_im)){
            res = count;
            break;
        }
        if((res & 31) == 31){
            saved_re = zre;
            saved_im = zim;
        }
    }
    output[index] = res;
}

// Host front-end function that allocates the memory and launches the GPU kernel
void host_fe(float upper_x,
             float upper_y,
             float lower_x,
             float lower_y,
             int *img,
             int res_x,
             int res_y,
             int max_iterations)
{
    if (res_x <= 0 || res_y <= 0)
        return;

    float step_x = (upper_x - lower_x) / (float)res_x;
    float step_y = (upper_y - lower_y) / (float)res_y;
    int *output;
    int *res;
    size_t pitch;
    // Set HW5_PROFILE=1 to measure the host-side cost of each stage.
    const bool profile = std::getenv("HW5_PROFILE") != nullptr;
    using Clock = std::chrono::steady_clock;
    const size_t row_bytes = static_cast<size_t>(res_x) * sizeof(int);
    const int batch_rows = std::min(res_y, 300);
    Clock::time_point t0, t1, t2, t3, free_start;
    std::chrono::duration<double, std::milli> d2h_time{}, memcpy_time{};
    if (profile) t0 = Clock::now();
    auto err = cudaHostAlloc((void **)&res, row_bytes * batch_rows, cudaHostAllocDefault);
    if (profile) t1 = Clock::now();
    auto err1 = cudaMallocPitch((void **)&output, &pitch, res_x * sizeof(int), res_y);
    if (profile) t2 = Clock::now();
    
    dim3 block(8,8);
    dim3 grid((res_x + block.x - 1) / block.x, (res_y + block.y - 1) / block.y);
    

    mandel_kernel<<<grid,block>>>(output, step_x, step_y, lower_x, lower_y, max_iterations, res_x, res_y, pitch);

    if (profile) {
        cudaError_t launch_err = cudaGetLastError();
        cudaError_t sync_err = cudaDeviceSynchronize();
        if (launch_err != cudaSuccess || sync_err != cudaSuccess)
            fprintf(stderr, "CUDA kernel error: %s / %s\n",
                    cudaGetErrorString(launch_err), cudaGetErrorString(sync_err));
        t3 = Clock::now();
    }


    for (int row = 0; row < res_y;) {
        const int rows = std::min(batch_rows, res_y - row);
        const char *src = reinterpret_cast<const char *>(output) + row * pitch;
        Clock::time_point start, copied;
        if (profile) start = Clock::now();
        cudaMemcpy2D(res, row_bytes, src, pitch, row_bytes, rows, cudaMemcpyDeviceToHost);
        if (profile) copied = Clock::now();
        memcpy(img + static_cast<size_t>(row) * res_x, res, rows * row_bytes);
        if (profile) {
            memcpy_time += Clock::now() - copied;
            d2h_time += copied - start;
        }
        row += rows;
    }
    if (profile) free_start = Clock::now();
    cudaFreeHost(res);
    cudaFree(output);

    if (profile) {
        auto t6 = Clock::now();
        auto ms = [](Clock::time_point a, Clock::time_point b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        };
        fprintf(stderr, "[kernel2 profile] host_alloc=%.3f gpu_alloc=%.3f "
                        "kernel_wait=%.3f d2h=%.3f memcpy=%.3f free=%.3f ms\n",
                ms(t0, t1), ms(t1, t2), ms(t2, t3), d2h_time.count(),
                memcpy_time.count(), ms(free_start, t6));
    }
}
