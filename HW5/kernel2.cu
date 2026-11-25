#include <cstdio>
#include <cstdlib>
#include <cuda.h>
#include <cstring>

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
    int res;
    for(res = 0 ; res < count; ++res){
        float re2 = zre * zre;
        float im2 = zim * zim;
        if(re2 * im2 > 4.f)
            break;
        float new_re = re2 - im2;
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
    size_t pitch;
    auto err = cudaHostAlloc((void **)&res, res_x*res_y*sizeof(int),cudaHostAllocDefault);
    auto err1 = cudaMallocPitch((void **)&output, &pitch, res_x * sizeof(int), res_y);
    
    dim3 block(32,32);
    dim3 grid((res_x + block.x - 1) / block.x, (res_y + block.y - 1) / block.y);
    

    mandel_kernel<<<grid,block>>>(output, step_x, step_y, lower_x, lower_y, max_iterations, res_x, res_y, pitch);

    // auto err2 = cudaDeviceSynchronize();


    cudaMemcpy2D(res, res_x*sizeof(int), output,pitch , res_x * sizeof(int), res_y, cudaMemcpyDeviceToHost);

    memcpy(img,res,res_x*res_y*sizeof(int));

    cudaFree(output);
    cudaFreeHost(res);
}
