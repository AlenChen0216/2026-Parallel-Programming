import subprocess

CASE = ["no-vectorized","vectorized","avx2"]
RUNS = 30
CMD = {
    "no-vectorized": "make clean && make && run --  ./test_auto_vectorize -t 1",
    "vectorized": "make clean && make VECTORIZE=1 && run --  ./test_auto_vectorize -t 1",
    "avx2": "make clean && make VECTORIZE=1 AVX2=1 && run --  ./test_auto_vectorize -t 1"
}
result = {
    "no-vectorized": [],
    "vectorized": [],
    "avx2": []
}

def parse_elasped_time(output):
    lines = output.splitlines()
    # the elapsed time is in the last line of the output
    elapsed_time_line = lines[-1]
    # the elapsed time is in the format "Elapsed time: x.xxxxxsec  (N: 1024, I: 20000000)"
    elapsed_time = float(elapsed_time_line.split("sec")[0])
    return elapsed_time

for case in CASE:
    for i in range(RUNS):
        print(f"Running {case} case, iteration {i+1}/{RUNS}")
        process = subprocess.Popen(CMD[case], shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = process.communicate()
        if process.returncode != 0:
            print(f"Error running {case} case: {stderr.decode()}")
            continue
        elapsed_time = parse_elasped_time(stdout.decode())
        result[case].append(elapsed_time)

print("Results:")
print(result)

# 1. median of each case
median_times = {}
to_file_data = {}
for case in CASE:
    times = result[case]
    times.sort()
    median_time = times[len(times) // 2]
    print(f"Median elapsed time for {case}: {median_time:.6f} sec")
    median_times[case] = median_time
    to_file_data[case] = median_time
# 2. speedup between no-vec and vec
print(f"Speedup of vectorized over no-vectorized: {median_times['no-vectorized'] // median_times['vectorized']:.2f}x")
to_file_data["speedup_vectorized"] = median_times['no-vectorized'] // median_times['vectorized']
# 3. speedup between no-vec and avx2
print(f"Speedup of avx2 over no-vectorized: {median_times['no-vectorized'] // median_times['avx2']:.2f}x")
to_file_data["speedup_avx2"] = median_times['no-vectorized'] // median_times['avx2']

# possible bit width of vec

# possible bit width of avx2

print("Writing results to file...")
with open("results.txt", "w") as f:
    for key, value in to_file_data.items():
        f.write(f"{key}: {value}\n")