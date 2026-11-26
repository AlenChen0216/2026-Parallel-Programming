#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cuda.h>
#include <cstring>

#define N 16

__global__ void mandel_kernel(int *output, float step_x, float step_y, float x0, float y0, int count, int width, int height)
{
    // To avoid error caused by the floating number, use the following pseudo code
    //
    // float x = lowerX + thisX * stepX;
    // float y = lowerY + thisY * stepY;
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if (i >= width || j >= height)
        return;

    float x = x0 + ((float)i * step_x);
    float y = y0 + ((float)j * step_y);
    int index = ((j * width) + i);

    float zre = x, zim = y;
    int res;
    for(res = 0 ; res < count; ++res){
        if(zre * zre + zim * zim > 4.f)
            break;
        float new_re = (zre * zre) - (zim * zim);
        float new_im = 2.f * zre * zim;
        zre = x + new_re;
        zim = y + new_im;
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

    static const bool timing = [] {
        const char *value = std::getenv("HW5_KERNEL4_TIMING");
        return value && std::strcmp(value, "1") == 0;
    }();
    using Clock = std::chrono::steady_clock;
    const auto stamp = [] { return timing ? Clock::now() : Clock::time_point{}; };
    const auto start = stamp();
    float step_x = (upper_x - lower_x) / (float)res_x;
    float step_y = (upper_y - lower_y) / (float)res_y;
    int *output;
    int *res;

    res = new int[res_x * res_y];
    const auto host_allocated = stamp();
    auto err1 = cudaMalloc((void **)&output, res_x*res_y*sizeof(int));

    const auto device_allocated = stamp();

    dim3 grid(1 + (res_x - 1) / N, 1 + (res_y - 1) / N);
    dim3 block(N,N);

    mandel_kernel<<<grid,block>>>(output, step_x, step_y, lower_x, lower_y, max_iterations, res_x, res_y);

    auto err2 = cudaDeviceSynchronize();
    const auto kernel_done = stamp();

    cudaMemcpy(res, output, res_x*res_y*sizeof(int), cudaMemcpyDeviceToHost);

    const auto device_copied = stamp();
    memcpy(img,res,res_x*res_y*sizeof(int));

    const auto host_copied = stamp();
    cudaFree(output);
    delete[] res;

    if (timing) {
        const auto end = Clock::now();
        const auto ms = [](Clock::time_point a, Clock::time_point b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        };
        std::fprintf(stderr, "[kernel4 ms] host_setup=%.3f cudaMalloc=%.3f launch_wait=%.3f "
                     "device_to_host=%.3f copy_to_img=%.3f cleanup=%.3f total=%.3f\n",
                     ms(start, host_allocated), ms(host_allocated, device_allocated),
                     ms(device_allocated, kernel_done), ms(kernel_done, device_copied),
                     ms(device_copied, host_copied), ms(host_copied, end), ms(start, end));
    }
}
