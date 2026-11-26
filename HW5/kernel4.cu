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

    float step_x = (upper_x - lower_x) / (float)res_x;
    float step_y = (upper_y - lower_y) / (float)res_y;
    int *output;
    int *res;

    res = new int[res_x * res_y];
    auto err1 = cudaMalloc((void **)&output, res_x*res_y*sizeof(int));

    dim3 grid(1 + (res_x - 1) / N, 1 + (res_y - 1) / N);
    dim3 block(N,N);

    mandel_kernel<<<grid,block>>>(output, step_x, step_y, lower_x, lower_y, max_iterations, res_x, res_y);

    auto err2 = cudaDeviceSynchronize();


    cudaMemcpy(res, output, res_x*res_y*sizeof(int), cudaMemcpyDeviceToHost);

    memcpy(img,res,res_x*res_y*sizeof(int));

    cudaFree(output);
    delete[] res;
}
