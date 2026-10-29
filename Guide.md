這份指南帶你完成 HW3 Part 1 的第一輪效能分析與 OpenMP 優化：先用 perf 找到 hot spot，再判斷適用的 OpenMP 功能，最後一次實驗一個優化方向。程式片段是局部示範，請配合每一步的問題自行修改與量測。

依照[作業 Part 1 要求](https://nycu-sslab.github.io/PP-f26/assignments/HW3/#31-part-1-requirements)，提交的程式修改限於 [`HW3/part1/cg_impl.c`](HW3/part1/cg_impl.c)，可以在其中增加變數或函式。以下指令以 repository 根目錄為起點，工作站環境使用 Bash。

**第一步：建立可以比較的 baseline**

先讀這四個位置，掌握呼叫關係：

| 位置 | 要讀懂的事情 |
| --- | --- |
| [`cg.c`](HW3/part1/cg.c) | `init()` → 一次暖機 `iterate()` → 正式執行 `NITER` 次 `iterate()` |
| [`cg_impl.c`](HW3/part1/cg_impl.c) 的 `iterate()` | 呼叫 `conj_grad()`，計算內積，再把 `z` 正規化成下一次使用的 `x` |
| `conj_grad()` | 每次呼叫執行 25 輪 CG；每輪包含稀疏矩陣乘向量、內積與向量更新 |
| [`grade.c`](HW3/part1/grade.c) | correctness、計分時間與執行緒數的取得方式 |

先在尚未加入 OpenMP 平行化的版本留下數據。Makefile 已有 `-g -O3 -fopenmp`，因此可以分析接近實際提交時的程式；保留 `-O3`，不需要為了 profiling 改成 `-O0`。

```bash
# 在課程工作站的 Bash 執行；自己的電腦不需要 module load pp。
module load pp
cd HW3/part1
make clean
make DATASIZE=MEDIUMN

# 保留量測資料與目前的 baseline；後續指令請在同一個 shell 執行。
PROFILE_DIR="$(mktemp -d /tmp/hw3-part1.XXXXXX)"
cp cg "$PROFILE_DIR/cg-baseline"
cp cg_impl.c "$PROFILE_DIR/cg_impl.baseline.c"
printf 'Profiling output: %s\n' "$PROFILE_DIR"

run -c 1 -- env OMP_NUM_THREADS=1 ./cg
```

`MEDIUMN` 是 Makefile 使用的實際拼法；更換資料大小前要 `make clean`。先使用作業預設大小，並確認 `VERIFICATION SUCCESSFUL`。

工作站的 `run -c N` 配置實體 CPU 資源，`OMP_NUM_THREADS=N` 控制 OpenMP team 的執行緒數，兩者都要設定。作業說明指出最多可配置 6 個；初期先使用 1、2、3、4 個。自己的 Linux 電腦沒有 `run` 時，移除 `run -c N --` 即可，例如 `env OMP_NUM_THREADS=1 ./cg`。

時間欄位也要先分清楚：

| 數據 | 包含範圍 | 用途 |
| --- | --- | --- |
| `Initialization time` | `init()`、一次暖機，以及重設 `x` | 判斷初始化的成本 |
| `Execution time` | 正式的 `NITER` 次 `iterate()` | 判斷 CG 計算階段是否改善 |
| perf 的 `seconds time elapsed` | 整個被測程序，包括啟動與輸出 | 比較完整程序的 wall time |
| grader 的 `student time` | 初始化、暖機與正式迭代的計時總和 | 最後評分時要關注的數據 |

目前 `cg.c` 的 `t_total` 沒有先初始化就使用 `+=`，因此它印出的 `Total Time` 不宜作為可靠依據。可用初始化時間加上正式執行時間輔助判讀，並以 grader 的 `student time` 確認；`grade.c` 有把累加器初始化為零。

**第二步：用 perf 找出時間花在哪裡**

先看整體計數與時間，重複三次觀察波動：

```bash
run -c 1 -- env OMP_NUM_THREADS=1 perf stat -r 3 \
    -e task-clock,context-switches,cpu-migrations,page-faults \
    ./cg

run -c 1 -- env OMP_NUM_THREADS=1 perf stat -r 3 \
    -e cycles:u,instructions:u,cache-references:u,cache-misses:u \
    ./cg
```

第一組先回答「多久、用了多少 CPU、是否有大量換頁或排程活動」；第二組再觀察使用者空間的硬體事件。perf 的[官方計數工具文件](https://github.com/torvalds/linux/blob/v6.1/tools/perf/Documentation/perf-stat.txt)說明了事件選擇與重複量測。

讀數據時，先記下 `seconds time elapsed`、`task-clock` 和 IPC（instructions / cycles）。`task-clock` 是累積的 CPU 時間；多執行緒時可能增加，但 wall time 同時下降。`page-faults` 包含常見的首次配置實體頁面，不代表一定發生磁碟 I/O；`cache-misses` 也不能直接證明已達記憶體頻寬上限。

再用取樣定位函式與程式碼：

```bash
run -c 1 -- env OMP_NUM_THREADS=1 perf record \
    -e cycles:u -F 499 --call-graph dwarf \
    -o "$PROFILE_DIR/baseline.data" -- ./cg

perf report -i "$PROFILE_DIR/baseline.data" --stdio --no-children

perf annotate -i "$PROFILE_DIR/baseline.data" \
    --stdio --symbol conj_grad --source

perf annotate -i "$PROFILE_DIR/baseline.data" \
    --stdio --symbol sparse --source
```

`record` 儲存樣本，`report` 顯示它們集中在哪些函式，`annotate` 對照原始碼與組合語言。`--no-children` 讓你先看函式本身的樣本比例，避免把呼叫者包含子函式的比例重複相加。Makefile 的 `-g` 提供除錯資訊，`--call-graph dwarf` 適合這種未特別保留 frame pointer 的建置。可參考 Linux 原始碼中的 [perf record 文件](https://github.com/torvalds/linux/blob/v6.1/tools/perf/Documentation/perf-record.txt)。

請在修改原始碼、重新編譯之前先讀完這次 report / annotate。舊資料對應的是舊執行檔與原始碼；比較不同版本時應保留對應檔案，避免行號或反組譯不匹配。

如果硬體事件不可用，可改用軟體事件：

```bash
run -c 1 -- env OMP_NUM_THREADS=1 perf record \
    -e cpu-clock:u -F 499 --call-graph dwarf \
    -o "$PROFILE_DIR/baseline-clock.data" -- ./cg
```

軟體事件也被權限限制時，先在允許 perf 的課程環境量測。`cpu-clock` 和 `cycles` 的取樣意義不同，比較前後版本時要使用相同事件。

撰寫本指南時，用目前程式碼、`MEDIUMN` 與 Makefile 相同的編譯選項，在本機暫存目錄做了一次單執行緒取樣：545 個 cycles 樣本中，`conj_grad()` 約占 75.03%，`sparse()` 約占 24.25%，且驗證通過。這是一次本機觀察，樣本數有限，請在工作站重新確認；不能直接當成工作站的效能結果。

接著請自己回答這些問題，並把答案記在量測筆記：

1. `conj_grad()` 和 `sparse()` 各占多少樣本？初始化是否值得另外優化？
2. `conj_grad()` 裡，樣本是否集中在 `a[k] * p[colidx[k]]` 附近？
3. 剩餘熱點是其他向量迴圈，還是初始化中的資料搬移與搜尋？

`-O3` 可能把指令重排、向量化，樣本也可能落在迴圈控制指令附近。請以整段迴圈判讀，避免只因某個 `add` 或 `cmp` 百分比較高，就認定它是根本瓶頸。

先分析 `./cg`，因為 `cg_grader` 會依序執行你的版本、reference 與 default。直接對 grader 取樣，會把三份程式的成本混在一起。

**第三步：把 hot spot 對應到資料相依性與 OpenMP 功能**

先看 `conj_grad()` 內這段 CSR 稀疏矩陣乘向量（SpMV）：

```c
for (int j = 0; j < lastrow - firstrow + 1; j++) {
    sum = 0.0;
    for (int k = rowstr[j]; k < rowstr[j + 1]; k++) {
        sum += a[k] * p[colidx[k]];
    }
    q[j] = sum;
}
```

`rowstr[j]` 到 `rowstr[j+1]` 是第 `j` 列的非零元素範圍，`colidx[k]` 指向要讀的向量元素。不同列可以共同讀取 `p`，而每列只寫自己的 `q[j]`；因此外層 `j` 是自然的平行單位。

新增 pragma 前，請先逐項判斷：「這個變數只有讀取嗎？每個 thread 寫的位置是否不同？有沒有累加到同一個 scalar？」

| 程式區段 | 相依性 | 可使用的功能 |
| --- | --- | --- |
| `q = A * p`、結尾的 `r = A * z` | 列與列獨立；列內有局部累加 | 外層 `parallel for`，列內區域變數 |
| `rho = r.r`、`d = p.q` | 所有項目累加到單一 scalar | `reduction(+:rho)`、`reduction(+:d)` |
| `z/r/p` 的逐元素更新 | 每個 `j` 寫自己的元素 | `parallel for`；後續可融合迴圈 |
| 最後的 residual 平方和 | 每列結果再累加 | `reduction(+:sum)`，差值使用區域變數 |
| `iterate()` 的兩個內積 | 同時累加兩個 scalar | `reduction(+:norm_temp1,norm_temp2)` |
| 已有 team 裡的多段工作 | 每段要共同分工並按相依性同步 | `parallel` + `for` + `single` |
| 連續陣列的逐元素運算、內積 | 可分配給 SIMD lanes | `simd`、`simd reduction`，量測效果 |

OpenMP 的 [worksharing loop 規格](https://www.openmp.org/spec-html/5.2/openmpse66.html)定義了 `for` 如何把 iteration 分給 team；[reduction 規格](https://www.openmp.org/spec-html/5.2/openmpsu52.html)則說明每份局部累加結果如何合併。

第一個重要陷阱是原本宣告在函式外層的 `sum`。如果只在原本的迴圈前加 `parallel for`，它預設仍是 shared，多個 thread 會一起覆寫。最容易檢查的寫法是把暫存變數宣告在列的 body 裡：

```c
#pragma omp parallel for schedule(static)
for (int j = 0; j < lastrow - firstrow + 1; j++) {
    double row_sum = 0.0;
    for (int k = rowstr[j]; k < rowstr[j + 1]; k++) {
        row_sum += a[k] * p[colidx[k]];
    }
    q[j] = row_sum;
}
```

這個模式中，`j` 是 OpenMP 迴圈的私有索引，`k` 和 `row_sum` 宣告在平行區域內；陣列資料共享，但 `q[j]` 的寫入位置互不重疊。結尾 `A*z` 裡原本的 `d` 也要用相同原則處理。若沿用外層變數，可使用 `private`，但 private 副本不會自動繼承原值，使用前仍須初始化。

第二個陷阱是內積。`private(rho)` 不會替你合併結果；逐項使用 `critical` 或 `atomic` 則會引入競爭。局部練習如下：

```c
rho = 0.0;
#pragma omp parallel for reduction(+:rho) schedule(static)
for (int j = 0; j < lastcol - firstcol + 1; j++) {
    rho += r[j] * r[j];
}
```

原本的 `rho` 也會參與最終合併，因此每次重新計算內積前必須歸零。浮點加法的結合順序可能改變，先檢查作業使用的相對誤差 `1e-10`，不要求每一位輸出完全相同。若誤差很大或結果不穩定，先查 race、重設與同步位置。

有兩種外層迴圈必須保留先後關係：

- `conj_grad()` 的 `cgit`：下一輪要使用這一輪更新後的 `p` 和 `rho`。
- `cg.c` 裡的 `it`：下一次 `iterate()` 要使用前一次產生的 `x`。

因此請平行化一輪內的資料工作。也不適合直接對 CSR 雙層迴圈加 `collapse(2)`：內層界限透過 `rowstr[j]` 決定，且同一列需要合併到同一個 `q[j]`；按列分工更直接。

**第四步：先實驗按列平行化 SpMV**

第一版先處理反覆執行的 `q = A*p`，以及結尾的 `A*z`，兩段都使用列內的區域累加變數。先選 `schedule(static)`，它通常能以較低的分派成本分配連續的列。

```bash
make
run -c 1 -- env OMP_NUM_THREADS=1 ./cg
run -c 2 -- env OMP_NUM_THREADS=2 ./cg
run -c 4 -- env OMP_NUM_THREADS=4 ./cg
```

每次都確認驗證成功。接著想一想：若單執行緒略慢、四執行緒變快，是否能用 OpenMP 的管理成本解釋？若四執行緒也沒改善，先確認 CPU 配額，再重新取樣，而不是繼續往其他迴圈加 pragma。

平行化後，熱點函式名稱可能變成 `conj_grad._omp_fn.0` 等 GCC 產生的 worker。請用 `perf report` 查實際名稱，再交給 `perf annotate --symbol`，不要只找原本的 `conj_grad`。

**第五步：加入 reduction，並嘗試融合迴圈**

SpMV 通過驗證後，再逐段處理內積和逐元素更新。先完成容易理解的版本，接著嘗試減少重複讀取與分工次數。

一個優先候選是把 `z/r` 更新與新的 `rho = r.r` 合併：原本先寫出新的 `r[j]`，下一個迴圈才重新讀取它；可以在更新當下累加。

```c
/* 先保存舊 rho，並確定 alpha 已經算好。 */
rho0 = rho;
rho = 0.0;
#pragma omp parallel for reduction(+:rho) schedule(static)
for (int j = 0; j < lastcol - firstcol + 1; j++) {
    z[j] += alpha * p[j];
    double r_new = r[j] - alpha * q[j];
    r[j] = r_new;
    rho += r_new * r_new;
}
/* 等迴圈與 reduction 完成後，才能用 rho / rho0 算 beta。 */
```

這段示範用來替換原本對應的更新與求和兩段；不要保留舊的求和，否則會重複累加。`rho0` 的保存位置也要一起檢查。

另一個候選是 `q=A*p` 完成每列後，立刻累加 `p[j]*q[j]`，用 `reduction(+:d)` 同時得到 `d`。請自行判斷這裡為什麼可行：這一輪的 `p` 是否始終唯讀？在計算某列的內積貢獻時，是否需要其他列的 `q`？

結尾的 `A*z` 與 residual 平方和也可用相似思路研究。這些融合要保留原本的陣列輸出與必要的長度界限；目前初始化把行列範圍都設為完整方陣，若改寫成共用長度，要先確認這個前提。

**第六步：減少反覆進出 parallel region 的成本**

如果每輪有多個 `parallel for`，25 輪 CG 就會反覆進出 team、分派工作並等待。執行期通常會重用 worker threads，但管理和同步仍有成本。

可以把 `conj_grad()` 的相關工作包在一個較大的 `parallel` region 裡，內部使用 `omp for` 分工，並以 `single` 處理共享 scalar。這一步結構變動較大，建議在前幾版驗證穩定後再做。

先把一輪相依關係寫出來：

```text
p 已就緒
  → q = A*p，取得 d = p.q
  → alpha = rho/d，保存 rho0，清零新的 rho
  → 更新 z、r，取得新的 rho
  → beta = rho/rho0
  → 更新 p
  → 下一輪 SpMV 可以讀取完整的新 p
```

請依序決定每段是 `omp for`、帶 reduction 的 `omp for`，還是 `omp single`。保留預設同步，先讓正確性通過，再研究是否有冗餘 barrier。

寫這一版時，特別檢查：

- 所有 threads 都走過相同順序的 worksharing constructs；`omp for` 本身不會建立新的 team。
- `cgit` 改成在 parallel region 內宣告的 thread 私有迴圈變數，讓每個 thread 依序走同樣的 25 輪。
- `rho`、`d` 等 worksharing reduction 目標必須在外層 team 共享；每列暫存值則保持私有。可用 `default(none)` 輔助逐一檢查變數歸屬。
- `d=0`、`rho=0` 等共享重設由單一 thread 執行並同步；若每個 thread 都重設，可能覆蓋已開始的累加。
- `alpha`、`beta`、`rho0` 由 `single` 計算後，後續 threads 必須看得到結果；`single` 預設有結尾 barrier，可參考 [single 規格](https://www.openmp.org/spec-html/5.2/openmpse62.html)。

先不要加 `nowait`。SpMV 會讀任意索引的 `p[colidx[k]]`，所以一個 thread 更新完自己的 `p` 區塊，並不代表下一輪已可開始。reduction 結果也必須完成才能被使用。融合迴圈若能直接消除一次工作分派，通常比先刪除相依迴圈之間的 barrier 更容易確認正確性。

**第七步：依量測選擇 scheduling、SIMD 或初始化優化**

到這裡再重新取樣，依新的熱點選下一個實驗：

| 觀察 | 可以先嘗試 | 要驗證的假設 |
| --- | --- | --- |
| SpMV 仍占大部分時間，列長差異大 | 比較 `static`、`dynamic,64`、`guided,64` | 負載平衡的收益是否大於排程成本 |
| 很多樣本在 `libgomp`、`GOMP_*` 等 runtime | 減少 parallel regions、融合迴圈，檢查負載分配 | runtime 樣本是否來自管理或等待；也可能是工作不均 |
| 向量更新、內積成本開始明顯 | 試 `omp simd` 或 `parallel for simd reduction` | SIMD 是否改善吞吐量，並維持誤差範圍 |
| 執行緒增加但 wall time 接近持平 | 檢查 CPU 配額、同步、記憶體存取與 bandwidth | 哪個因素限制擴展，避免只由 cache miss 推斷 |
| 初始化成為較大比例 | 研究 `sparse()` 的清零與資料建構 | 初始化能否改善 grader 的總時間 |

若想用 `OMP_SCHEDULE` 切換策略，程式必須先改成 `schedule(runtime)`，例如：

```bash
run -c 4 -- env OMP_NUM_THREADS=4 OMP_SCHEDULE=static ./cg
run -c 4 -- env OMP_NUM_THREADS=4 OMP_SCHEDULE=dynamic,64 ./cg
run -c 4 -- env OMP_NUM_THREADS=4 OMP_SCHEDULE=guided,64 ./cg
```

固定寫成 `schedule(static)` 的迴圈不會因 `OMP_SCHEDULE` 改變。先看 `rowstr[j+1]-rowstr[j]` 的列長分布，再判斷是否值得用動態分派。環境變數語法可查 [GCC 的 OMP_SCHEDULE 文件](https://gcc.gnu.org/onlinedocs/libgomp/OMP_005fSCHEDULE.html)。

SIMD 可先從連續向量迴圈開始。SpMV 的內層也能研究 `omp simd reduction(+:row_sum)`，但 `p[colidx[k]]` 是間接存取，是否更快取決於硬體、編譯器與資料局部性，不能預設一定有效。參考 [simd 規格](https://www.openmp.org/spec-html/5.2/openmpse60.html)。

`sparse()` 裡註解為 `preload data pages` 的清零迴圈，是初始化階段比較容易起步的候選：每列的 `a/colidx` 範圍互不重疊，`nzloc[j]` 也各自獨立。平行 first touch 可能分攤觸碰頁面的成本，但通常不會讓必須配置的頁面數量消失；仍要看 wall time，不能只看 page-fault 計數。

初始化的其他部分有不同的相依性，不能比照清零直接平行化：

- `makea()` 呼叫 `sprnvc()` 時會更新共用亂數狀態 `tran`，同時還有共用暫存陣列；必須維持題目預期的矩陣生成結果。
- `rowstr` 與 `nzloc` 的 prefix sum 依賴前一項。
- `sparse()` 的矩陣插入流程會讓不同來源更新同一列，還有 `size *= ratio` 的遞推。
- 最後壓縮 `a/colidx` 是原地搬移；即使目的區間不重疊，也可能覆寫別列尚未讀取的來源。

如果 profiling 顯示矩陣建構值得深入優化，再研究如何改成明確的列所有權、額外緩衝或其他資料結構。這適合作為後續獨立實驗。

**第八步：用一致流程決定保留哪個版本**

每次只改一個主要想法，保留前一版來源與量測結果。重新編譯後，先做 correctness，再分開測時間與取樣：

```bash
make

# 檢查 correctness 與 scaling；每個設定都重複三次。
for t in 1 2 3 4; do
    for trial in 1 2 3; do
        run -c "$t" -- env OMP_NUM_THREADS="$t" ./cg \
            > "$PROFILE_DIR/current-t${t}-r${trial}.log"
    done
done

rg 'VERIFICATION|Initialization time|Execution time' "$PROFILE_DIR"/*.log

# 以四個 threads 的實際總時間檢查波動。
run -c 4 -- env OMP_NUM_THREADS=4 perf stat -r 3 ./cg

# 另一次執行用來定位新的 hot spot。
run -c 4 -- env OMP_NUM_THREADS=4 perf record \
    -e cycles:u -F 499 --call-graph dwarf \
    -o "$PROFILE_DIR/current-t4.data" -- ./cg
perf report -i "$PROFILE_DIR/current-t4.data" --stdio --no-children

# 最後核對課程 grader；不要同時執行其他 benchmark。
run -c 4 -- env OMP_NUM_THREADS=4 ./cg_grader
```

不要把 `perf record` 這次執行的 wall time 當正式速度結果；收集 call stack 會有額外成本。測試期間也不要同時跑不同版本，以免它們互搶 CPU 與記憶體頻寬。

grader 除了要求數值驗證通過，還會檢查 `student time` 必須比 `default time` 至少快約 0.1 秒；詳見 `grade.c`。所以「輸出驗證成功」和「已達到計分所需的速度」要分別確認。

可以用這張表記錄每版三次量測的中位數，時間欄位請固定使用相同定義：

| 版本 | 主要改動 | 1 thread 時間 | 2 threads 時間 | 3 threads 時間 | 4 threads 時間 | 全部驗證通過 | grader student time |
| --- | --- | --- | --- | --- | --- | --- | --- |
| baseline | 原始序列版本 | 待測 | 待測 | 待測 | 待測 | 待確認 | 待測 |
| v1 | 按列平行化兩段 SpMV | | | | | | |
| v2 | 內積 reduction、向量更新 | | | | | | |
| v3 | 融合相鄰迴圈 | | | | | | |
| v4 | 共用 parallel region | | | | | | |
| v5 | 依新熱點選一項優化 | | | | | | |

比較時同時記錄 `T_serial / T_parallel(N)`，看相對原始版本的改善；也記錄 `T_parallel(1) / T_parallel(N)`，看修改後程式的 scaling。即使正式 CG 計算明顯加速，序列初始化仍會限制整體收益。

你現在可以先完成三個小成果：保存 baseline 的 perf 結果、寫下 SpMV 每個變數的讀寫歸屬、實作並量測 v1。確認這三項後，再進入 reduction 與迴圈融合，讓每次修改都有可以解釋的結果。
