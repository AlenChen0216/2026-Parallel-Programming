# Programming Assignment II: Multi-thread Programming

## Q1

### In your write-up, plot a graph of speedup (compared to the reference sequential implementation) as a function of the number of threads used for VIEW 1. Is the speedup linear to the number of threads used? Hypothesize why this is (or is not) the case. (You may want to plot a graph for VIEW 2 for further insights.)


我的程式碼如下：
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

實作時，我將影像由上而下切分成數個連續的列區段，讓每個 thread 負責其中一段。各 thread 分配到的列數盡可能相同，剩餘的列則由最後一個 thread 計算。

不同 thread 數量下的 speedup 如下圖：

![speedup1](./pic/PP2-speedup_1.png)

從圖中可以看出，使用 3 個 thread 時的 speedup 低於使用 2 個 thread 時的結果。因此，這組測量結果並未呈現 speedup 隨 thread 數量線性增加的趨勢。可能的原因是各 thread 的實際計算量不同，導致部分 thread 必須花較久時間完成工作；Q2 將進一步檢視這個可能性。

## Q2

### How do the measurements explain the speedup graph you previously plotted?

我在 `worker_thread_start` 中記錄各 thread 執行 `mandelbrot_serial` 的時間。當使用 3 個 thread 時，thread 1 的執行時間明顯較長，如下圖所示：

![too_high](./pic/why.png)

再查看各 thread 負責區域所生成的圖片，可以看到 thread 1 的區域中，白色像素的比例較高：

thread 0:
![thrad0](./pic/mandelbrot-thread-0.png)

thread 1:
![thrad1](./pic/mandelbrot-thread-1.png)

thread 2:
![thrad2](./pic/mandelbrot-thread-2.png)

在輸出圖片中，像素亮度會隨該像素的 iteration 次數增加。

而 `mandel` 函數會反覆計算，直到 `z_re * z_re + z_im * z_im > 4.f`，或達到最大 iteration 次數。因此，較亮的區域表示這些像素需要較多次迭代才停止，或在上限內沒有觸發停止條件。這讓 thread 1 的較長執行時間與其負責區域中較多的高 iteration 像素相互吻合，顯示各區域的工作量可能不均。

由於影像是按列切分，像素數量相近不代表所需的迭代次數相近。若 thread 1 負責的區域需要較多計算，其他 thread 即使較早完成，整體執行時間仍會受到較慢 thread 的限制。這可能解釋為何使用 3 個 thread 時的 speedup 反而低於使用 2 個 thread 的結果。

## Q3

### In your write-up, describe your parallelization approach and report the final speedup achieved with 4 threads.



## Q4

### Is the performance noticeably better than with 6 threads? Why or why not?


