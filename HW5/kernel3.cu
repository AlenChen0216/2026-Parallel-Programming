#include <algorithm>
#include <cstring>
#include <cuda.h>

#define N   8
#define STP 2

__global__ void mandel_kernel(int *output,
                              const float step_x,
                              const float step_y,
                              const float x0,
                              const float y0,
                              const int count,
                              const int width,
                              const int height,
                              const size_t pitch)
{
    // To avoid error caused by the floating number, use the following pseudo code
    //
    // float x = lowerX + thisX * stepX;
    // float y = lowerY + thisY * stepY;
    int init_j = (threadIdx.x + blockIdx.x * blockDim.x) * STP;
    int init_i = (threadIdx.y + blockIdx.y * blockDim.y) * STP;
    int padding = pitch / sizeof(int);

    for (int i = init_i; i < min(init_i + STP, height); i++)
    {
        for (int j = init_j; j < min(init_j + STP, width); j++)
        {
            float x = x0 + ((float)j * step_x);
            float y = y0 + ((float)i * step_y);
            int index = ((i * padding) + j);
            float zre = x, zim = y;
            float saved_re = zre, saved_im = zim;
            int res;
            for (res = 0; res < count; ++res)
            {
                float re2 = zre * zre;
                float im2 = zim * zim;
                if (re2 + im2 > 4.f)
                    break;
                float new_re = re2 - im2;
                float new_im = 2.f * zre * zim;
                zre = x + new_re;
                zim = y + new_im;
                if (__float_as_uint(zre) == __float_as_uint(saved_re)
                    && __float_as_uint(zim) == __float_as_uint(saved_im))
                {
                    res = count;
                    break;
                }
                if ((res & 31) == 31)
                {
                    saved_re = zre;
                    saved_im = zim;
                }
            }
            output[index] = res;
        }
    }

    // if (i >= width || j >= height)
    //     return;
    // int padding = pitch / sizeof(int);
    // float x = x0 + ((float)i * step_x);
    // float y = y0 + ((float)j * step_y);
    // int index = ((j * padding) + i);

    // float zre = x, zim = y;
    // float saved_re = zre, saved_im = zim;
    // int res;
    // for(res = 0 ; res < count; ++res){
    //     float re2 = zre * zre;
    //     float im2 = zim * zim;
    //     if(re2 + im2 > 4.f)
    //         break;
    //     float new_re = re2 - im2;
    //     float new_im = 2.f * zre * zim;
    //     zre = x + new_re;
    //     zim = y + new_im;
    //     if(__float_as_uint(zre) == __float_as_uint(saved_re) && __float_as_uint(zim) ==
    //     __float_as_uint(saved_im)){
    //         res = count;
    //         break;
    //     }
    //     if((res & 31) == 31){
    //         saved_re = zre;
    //         saved_im = zim;
    //     }
    // }
    // output[index] = res;
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
    const size_t row_bytes = static_cast<size_t>(res_x) * sizeof(int);
    const int batch_rows = std::min(res_y, 200);
    auto err = cudaHostAlloc((void **)&res, row_bytes * batch_rows, cudaHostAllocDefault);
    auto err1 = cudaMallocPitch((void **)&output, &pitch, res_x * sizeof(int), res_y);

    dim3 block(N, N);
    const unsigned tile_width = block.x * STP;
    const unsigned tile_height = block.y * STP;
    dim3 grid(1 + (res_x - 1) / tile_width, 1 + (res_y - 1) / tile_height);

    mandel_kernel<<<grid, block>>>(output, step_x, step_y, lower_x, lower_y, max_iterations, res_x,
                                   res_y, pitch);

    for (int row = 0; row < res_y;)
    {
        const int rows = std::min(batch_rows, res_y - row);
        const char *src = reinterpret_cast<const char *>(output) + row * pitch;
        cudaMemcpy2D(res, row_bytes, src, pitch, row_bytes, rows, cudaMemcpyDeviceToHost);
        memcpy(img + static_cast<size_t>(row) * res_x, res, rows * row_bytes);
        row += rows;
    }
    cudaFreeHost(res);
    cudaFree(output);
}
