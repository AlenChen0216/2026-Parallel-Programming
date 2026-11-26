#include <cuda.h>
#include <cstring>

#define N 8

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

    const float init_x = x0 + ((float)i * step_x);
    const float init_y = y0 + ((float)j * step_y);
    int index = ((j * width) + i);

    float x = init_x, y = init_y;
    float x2 = x * x, y2 = y * y;
    int res;
    for (res = 0; res < count; ++res) {
        if (x2 + y2 > 4.f)
            break;
        y = 2.f * x * y + init_y;
        x = (x2 - y2) + init_x;
        x2 = x * x;
        y2 = y * y;
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
