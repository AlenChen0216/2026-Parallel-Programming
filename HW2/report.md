# Programming Assignment II: Multi-thread Programming

## Q1

我的 code 如下所示:
```c++
void worker_thread_start(WorkerArgs *const args){
    int start_row, total_rows;
    start_row = (args->height / args->numThreads) * args->threadId;
    if (args->threadId == args->numThreads - 1){
        total_rows = args->height - start_row;
    }else{
        total_rows = args->height / args->numThreads;
    }
    mandelbrot_serial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, start_row, total_rows, args->maxIterations, args->output);
}
```
實做主要是從上到下讓每一個 thread 計算獨立的區塊，且區塊面積盡可能一致。

使用不同 thread 數量的 speedup 如下圖:

![speedup1](./)

根據結果圖來看，thread 使用數量為 3 時 speedup 相比 thread 使用數量為 2 時少，因此 speedup 跟 thread 使用數量不為線性關係。

我認為是因為 thread 使用數量為 3 時，其中一個 thread 的 workload 較高所導致的。

## Q2


## Q3


## Q4


