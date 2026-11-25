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

---

**Part 2：BFS 實作引導**

這一部分依序處理 top-down optimization、bottom-up、hybrid。依照[作業要求](https://nycu-sslab.github.io/PP-f26/assignments/HW3/#32-part-2-requirements)，BFS 的提交修改集中在 [`bfs.cpp`](HW3/part2/breadth_first_search/bfs.cpp)；可以在其中新增 helper、調整自己新增的 helper 參數。三個公開的 `bfs_*()` 函式介面請保持一致。

目前你的 top-down 只有初始化 `distances` 的迴圈加了 OpenMP；`top_down_step()` 仍是序列版。`bottom_up_step()`、`bfs_bottom_up()` 和 `bfs_hybrid()` 尚待實作。下面以這個狀態為起點，提供局部片段與練習。

**先固定 BFS 每一層的規則**

令 `depth = d`，在一層開始時：

- `frontier` 恰好包含距離 root 為 `d` 的頂點，每個頂點只出現一次。
- 本層只能把尚未訪問的頂點設成 `d+1`，並放進 `next_frontier`。
- 整層完成後，才交換兩個 frontier，增加 `depth`。
- root 的距離是 `0`；搜尋結束後，不可達頂點仍為 `-1`。

層與層之間有相依性，先平行化一層裡面的工作。每個 BFS 函式都要自行重新初始化結果與工作空間，因為 runner / grader 會反覆呼叫它們。

| 方法 | 誰負責發現新頂點？ | 讀取的邊 | 主要競爭 |
| --- | --- | --- | --- |
| Top-down | 本層 frontier 的每個頂點 `u` | `outgoing(u)`：`u → v` | 不同 `u` 可能同時發現同一個 `v` |
| Bottom-up | 每個未訪問頂點 `v` 自己找父節點 | `incoming(v)`：`u → v` | 按 `v` 分工時，距離可由唯一 thread 寫入 |
| Hybrid | 每層選擇其中一種 step | 依本層方向決定 | 切換時的 frontier 表示與同步 |

題目的圖可以是有向圖。Bottom-up 查的是「誰有邊指向 `v`」，所以要用 `incoming_begin/end()`，不能直接沿用 top-down 的 outgoing edges。這些 helper 定義在 [`graph_internal.h`](HW3/part2/common/graph_internal.h)，也處理了最後一個頂點沒有 `starts[v+1]` 的情況。

**Top-down，第一版：先處理兩個獨立的 race**

先在紙上畫出這張圖：

```text
0 → 1 → 3
  ↘ 2 ↗
```

本層 frontier 是 `{1, 2}` 時，兩個 threads 都可能看到 `distances[3] == -1`。原本的「先判斷，再設定」不是不可分割的操作，直接加 `parallel for` 會讓頂點 `3` 被重複加入。

第一個需要保護的是「取得發現 `v` 的資格」。把判斷與寫入改成 CAS，只有成功者可以加入下一層：

```cpp
// 局部片段：depth 是本層距離，在整個 step 中唯讀。
if (__sync_bool_compare_and_swap(
        &distances[v], NOT_VISITED_MARKER, depth + 1)) {
    // 只有這個 thread 能把 v 加進 next_frontier。
}
```

這個 builtin 的語意可查 [GCC CAS 文件](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html)。同一層的發現者都會提出 `depth+1`，但仍需要 CAS 保證只加入一次；即使寫入數值相同，未同步的並行寫入也有問題。

第二個需要保護的是「分配輸出位置」。即使 CAS 保證不同 thread 加入不同頂點，它們仍可能同時執行 `new_frontier->count++`，拿到相同 index。第一版可以先用 OpenMP atomic capture：

```cpp
// 放在 CAS 成功的分支中。
int index;
#pragma omp atomic capture
index = new_frontier->count++;
new_frontier->vertices[index] = v;
```

atomic capture 保護 counter；取得 index 後，每個 thread 寫自己的位置即可。清空 count 必須在 step 開始前完成；讀取新 frontier、交換指標必須等整個 parallel step 完成。只有 count 更新完成，還不代表對應的 `vertices[index]` 已寫完。

現在才把 `top_down_step()` 外層的 frontier 索引迴圈加上 `parallel for`。先使用 `schedule(static)`，確認 1、2、4 threads 都得到相同距離，再考慮負載平衡。請維持列內的 `node`、邊範圍與鄰居索引為區域變數。

這一版的完成條件：菱形圖中的頂點 `3` 距離為 `2`，而且只加入一次；下一層容量不超過 `num_nodes`。Frontier 內的排列順序可以不同，BFS 距離必須一致。

**Top-down，第二版：減少不必要的 CAS 與共享 counter 更新**

先考慮作業提示：「何時已知道 CAS 一定失敗？」頂點若已訪問，就不需要再做 read-modify-write。可以在 CAS 前加快速檢查：

```cpp
if (__atomic_load_n(&distances[v], __ATOMIC_RELAXED)
        == NOT_VISITED_MARKER &&
    __sync_bool_compare_and_swap(
        &distances[v], NOT_VISITED_MARKER, depth + 1)) {
    // 收集新頂點。
}
```

快速檢查只用來跳過工作，不能取代 CAS；另一個 thread 可能在檢查後先成功。這裡使用 atomic load，是因為本層其他 threads 正在更新相同位置；混用普通讀取與並行原子寫入，不能只靠「int 很小」保證 C++ 資料競爭正確性。`RELAXED` load 只觀察標記，不用它發布其他資料；下一層的可見性仍由 step 結束的同步負責。可參考 [GCC atomic 文件](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html)。

接著處理每次成功發現都更新共享 count 的成本。做法是先收進 thread 私有的 buffer，再一次預約一段輸出空間：

```text
parallel region，每個 thread 建立自己的 local_vertices
    omp for：分配本層 frontier 頂點
        掃 outgoing edges
        CAS 成功 → local_vertices.push_back(v)

    每個 thread 取得 local_count
    用一次 atomic capture 預約 [base, base + local_count)
    將自己的 local_vertices 複製到 new_frontier 對應範圍
parallel region 結束：所有複製完成
```

預約區間的局部寫法是：

```cpp
int base;
int local_count = static_cast<int>(local_vertices.size());
#pragma omp atomic capture
{
    base = new_frontier->count;
    new_frontier->count += local_count;
}
// 然後複製到 new_frontier->vertices + base。
```

`std::vector<int>` 要建在每個 thread 的區域範圍，並在 `bfs.cpp` 引入 `<vector>`。一個共享 vector 的 `push_back()` 仍會 race。先用可成長的私有 vector，避免直接為每個 thread 配置 `num_nodes` 大小，讓記憶體膨脹為 threads × vertices。

第一版每個成功發現約做一次共享 counter 更新；這一版可減成每個 thread 每層一次。另一條路是每個 thread 留下 count，由 prefix sum 計算輸出 offset，再平行複製；等 atomic 預約版本穩定後再研究。

最後比較 `static` 與 `dynamic,64` / `guided,64`。Frontier 頂點數相近，不代表出邊數相近；高出度頂點可能讓某個 thread 特別忙。動態排程有管理成本，也無法把單一超大頂點的鄰接串列自動拆開。若 profiling 顯示單一 hub 主導，再研究按邊切分或處理高出度頂點。

**Bottom-up，第一版：先讓序列演算法正確**

這次的問題改成：「對每個尚未訪問的 `v`，是否有 `u → v`，而且 `u` 在本層 frontier？」找到第一個符合者就能停止搜尋，不必列舉所有可能的父節點。

Top-down 的 frontier list 適合列舉成員，bottom-up 則需要大量的 membership 查詢。第一版可以用兩個長度為 `num_nodes` 的 byte arrays：

```cpp
std::vector<unsigned char> current(graph->num_nodes, 0);
std::vector<unsigned char> next(graph->num_nodes, 0);
current[ROOT_NODE_ID] = 1;
```

配合全部距離初始化為 `-1`、root 設為 `0`，以 `depth=0` 開始。每層做：

```text
清空 next，new_count = 0
for 每個頂點 v:
    若 distances[v] != -1：跳過
    for u in incoming(v):
        若 current[u] == 1:
            distances[v] = depth + 1
            next[v] = 1
            new_count += 1
            break

所有頂點掃完後：
    current 與 next 交換
    depth += 1
    若 new_count == 0：搜尋結束
```

關鍵是 `current` 在整層固定不變。新發現的頂點只進入 `next`；如果立即修改 `current`，同一輪稍後掃到的頂點可能利用新成員繼續前進，讓一條鏈在一輪內跨越多層。

第一版也先避免用 `distances[u] == depth` 直接代替 membership：其他 threads 可能同時寫入原本未訪問的 `u`，即使邏輯判斷看似可以區分層數，普通讀寫仍可能形成 race。固定的 byte array 讓讀寫歸屬更清楚。

你目前自行新增的 `bottom_up_step(Graph, VertexSet*, int*)` 可以調整參數，例如加入唯讀的 `current`、可寫的 `next`、`depth`，並回傳新發現數量。這是 `bfs.cpp` 內的 helper；不必讓它勉強沿用缺少必要資訊的介面。

先在有向鏈 `0 → 1 → 2 → 3` 上手算並核對結果 `[0,1,2,3]`，再測菱形圖、孤立頂點、反向邊與環。

**Bottom-up，第二版：按目的頂點平行化**

在上面的外層 `v` 迴圈加入 `parallel for reduction(+:new_count)`，先用 `schedule(static)`。只要每個 `v` 由唯一 thread 處理，就有以下所有權：

| 資料 | 本層的存取方式 |
| --- | --- |
| `current[u]` | 所有人只讀 |
| `distances[v]` | 負責 `v` 的 thread 讀寫；鄰居判斷改讀 `current` |
| `next[v]` | 負責 `v` 的 thread 寫入 |
| `new_count` | thread 局部累加，最後 reduction |

因此更新 `distances[v]` 不需要 CAS。若另外維護下一層 list，共享 append 的問題仍然存在，可重用 top-down 的私有 buffer 與批次合併方法。

先使用 byte array，讓不同 `v` 對應不同記憶體位置。`std::vector<bool>` 與手工 packed bitmap 會把多個頂點塞進同一個 word；不同 threads 設定不同 bit 也可能競爭，需要額外的 word 分工或 atomic bit operation。

每一層都要清理 `next`，包含已訪問頂點對應的位置；只寫新發現者而留下舊標記，交換後會混入過期的 frontier。交換與停止條件都放在 parallel loop 完成之後。

優化時先保留找到第一個父節點就 `break` 的設計。後續可比較排程、維護 unvisited list，以及融合每個 `v` 的清零與搜尋；先確認 byte array 的生命週期正確，再增加資料結構。

Bottom-up 並非所有圖都快。每層至少要掃候選頂點，frontier 很小時還可能檢查大量 incoming edges 才發現沒有父節點。長鏈、網格等多層圖尤其需要觀察這個成本。

**Hybrid，第一版：共用狀態，每層選一個 step**

先讓獨立的 top-down 與 bottom-up 都通過 correctness，再做 hybrid。Hybrid 需要一個共同的 BFS 外層迴圈，保留相同的 `distances`、`depth` 與 frontier。每層選擇一個 step；如果呼叫完整的 `bfs_top_down()` 或 `bfs_bottom_up()`，會重新初始化並從 root 重跑。

第一版為了降低切換複雜度，可以要求兩種 step 都產生下一層的 vertex list：

- Top-down 直接使用 list；只有選到 bottom-up 時，才把本層 list 轉成唯讀 membership array。
- Bottom-up 掃描未訪問頂點，讀 membership，並以私有 buffer 收集下一層 list。
- 整層完成後交換 list。下一層再決定方向；切換不改動已得到的距離。

這樣先用一份 list 作為層邊界的共同表示。Bottom-up 若長時間持續執行，再研究保留兩份 dense arrays，只在切回 top-down 時轉成 list，以減少中間的轉換與收集成本。

先試容易理解的 frontier 比例門檻，例如：top-down 模式下，`frontier_count > 0.05 * num_nodes` 時切到 bottom-up；bottom-up 模式下，frontier 已縮小且低於 `0.02 * num_nodes` 時切回。這些數值只是第一個實驗假設，請由量測調整，並用浮點比例或足夠寬的整數乘法避免小圖整除問題。

兩個方向採不同門檻，並觀察 frontier 是否縮小，可以避免在邊界附近反覆切換。門檻判斷與表示轉換都在層邊界執行。

**Hybrid，第二版：依預估邊數選方向**

Frontier 大小只計算頂點數；少數高出度頂點也可能產生很多工作。可改為追蹤：

```text
n_f = 本層 frontier 的頂點數
m_f = Σ outgoing_size(g, u)，u 在本層 frontier
m_u = Σ incoming_size(g, v)，v 尚未訪問
```

在這份有向圖介面中，`m_f` 是 top-down 預計檢查的邊數，`m_u` 是 bottom-up 在沒有提早 break 時的邊檢查上界；它們都沒有完整計入頂點掃描、原子操作與表示轉換時間。

可以研究 `m_f > m_u / alpha` 時切到 bottom-up，frontier 縮小且 `n_f < num_nodes / beta` 時切回。[隨作業提供的論文](HW3/part2/doc/bfs.pdf)第五節解釋了這類控制方式，第六節使用 `alpha=14`、`beta=24` 做實驗。這些是論文實驗的參數，可作為起點；在本作業的有向圖與工作站上仍要重新評估。

計數可以用 `int64_t`，並在累加前轉成寬整數。取得新 frontier 時順便累加其出度；`m_u` 可在初始化時求和，之後減掉本層新訪問頂點的入度，避免只為了選方向就多掃整張圖。這些更新也要使用 reduction 或 thread 私有統計。

Hybrid 的第一個觀察目標是：小 frontier 採 top-down，中間密集層採 bottom-up，尾端視收益切回。若某張圖始終維持小 frontier，整場使用 top-down 也可能合理，不需要強迫每張圖都切換。

**測試與量測：從小圖確認，再看每一層的工作**

先建立最小圖，測各種容易出錯的情況：

| 圖 | 要確認的性質 |
| --- | --- |
| 單一 root、無邊 | 只得到距離 `0`，正常結束 |
| 有向鏈 `0→1→2→3` | 距離恰為 `[0,1,2,3]`，不跨層 |
| 菱形 `0→1,0→2,1→3,2→3` | `3` 距離為 `2`，只加入一次 |
| 星狀圖 | 大 frontier 的容量、輸出合併與負載分配 |
| 只有 `1→0`，加一個孤立點 `2` | 距離 `[0,-1,-1]`，incoming/outgoing 方向正確 |
| 環、自環、重複邊 | 不重複發現，不無限迴圈 |

工具支援將文字圖轉成 binary。先用附帶的五頂點圖熟悉流程：

```bash
# 從 repository 根目錄開始；工作站先 module load pp。
cd HW3/part2/breadth_first_search
make
make -C ../tools
BFS_WORK="$(mktemp -d /tmp/hw3-bfs.XXXXXX)"
../tools/graph_tools text2bin ../tools/plaintext.graph "$BFS_WORK/tiny.graph"
run -c 4 -- env OMP_NUM_THREADS=4 ./bfs "$BFS_WORK/tiny.graph" 4
```

[`plaintext.graph`](HW3/part2/tools/plaintext.graph) 的 root 直接連到其他四個頂點，所以預期距離為 `[0,1,1,1,1]`。可仿照這個檔案格式在暫存目錄建立自己的測例，再用 `text2bin` 轉換。

目前 [`main.cpp`](HW3/part2/breadth_first_search/main.cpp) 的 usage 字串寫了指定 threads 時沒有 correctness run，但實際兩條分支都會執行 reference 並比較距離；請以程式碼與實際輸出為準。測試途中未完成的 bottom-up / hybrid 會報錯，先確認正在實作的項目；三者完成後應全部通過。

接著在工作站依序測 1、2、4 threads，先用小圖，再用大型圖：

```bash
GRAPH_DIR=/data/pp/hw3/test
for t in 1 2 4; do
    run -c "$t" -- env OMP_NUM_THREADS="$t" \
        ./bfs "$GRAPH_DIR/com-orkut_117m.graph" "$t"
done

# 最後測完整計分圖集合；-r 3 的 grader 會取三次中的最小時間。
run -c 4 -- env OMP_NUM_THREADS=4 \
    ./bfs_grader -n 4 -r 3 "$GRAPH_DIR"
```

目前 grader 的圖集合包含 `grid1000x1000.graph`、`soc-livejournal1_68m.graph`、`com-orkut_117m.graph`、`random_500m.graph`、`rmat_200m.graph`。不要只用一張低直徑圖選門檻；也要看網格圖中的逐層掃描成本。

可以在除錯模式下每層記錄 `depth`、方向、frontier 大小、實際檢查邊數、CAS 嘗試數與成功數、表示轉換時間及 step 時間。大量統計用 thread 局部累加與 reduction，避免逐邊更新共享 counter 改變測試結果；正式測時間時關閉統計輸出。

`bfs.cpp` 已有 `VERBOSE` 區塊，但目前裡面的 include 是 `../common/CycleTimer.h`，實際檔名是 `cycle_timer.h`；在 Linux 啟用前要於 `bfs.cpp` 修正大小寫，並用 `-DVERBOSE` 編譯。現有的逐層輸出只涵蓋 top-down，其他方法需要自行加入相同觀察點。

若用 perf：

```bash
run -c 4 -- env OMP_NUM_THREADS=4 perf record \
    -e cycles:u -F 499 --call-graph dwarf \
    -o "$BFS_WORK/bfs.data" -- \
    ./bfs "$GRAPH_DIR/com-orkut_117m.graph" 4
perf report -i "$BFS_WORK/bfs.data" --stdio --no-children
```

這份 profile 包含載圖、三個 student BFS 與三個 reference BFS。閱讀時要定位自己的 `bfs_top_down`、`bfs_bottom_up`、`bfs_hybrid` 及其 worker/helper；不要把全程序時間當成某一個 BFS 的時間。若各方法共用 helper，整體 profile 也會合併其成本，這時逐層計時，或暫存目錄中只呼叫單一版本的 driver，更容易區分。正式效能比較使用 runner / grader 計算的各函式時間。

| 實作版本 | 要驗證的改變 | 完成條件 |
| --- | --- | --- |
| TD v1 | CAS + atomic append | 多執行緒距離正確、frontier 無重複 |
| TD v2 | atomic 預檢 + thread 私有 buffer | correctness 維持，觀察原子操作與時間是否下降 |
| BU v1 | 固定 current / next，序列掃 incoming | 鏈、反向邊、不可達點都正確 |
| BU v2 | 按 `v` 平行化 + count reduction | 1、2、4 threads 結果一致 |
| Hybrid v1 | frontier 比例切換，共用 list | 切換前後層數與距離正確 |
| Hybrid v2 | 邊數估計、降低轉換成本 | 多種圖的總時間改善，而非只改善單層 |

現在先實作 **TD v1**：在 frontier 迴圈分工，以 CAS 決定發現資格，以 atomic capture 分配輸出位置。驗證菱形圖後，再比較 per-thread buffer；接著進入 bottom-up，最後組合成 hybrid。
