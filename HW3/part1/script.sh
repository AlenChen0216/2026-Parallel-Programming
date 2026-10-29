for n in 1 2 4 8; do
    echo "OMP_NUM_THREADS=$n"
    OMP_NUM_THREADS=$n OMP_DYNAMIC=FALSE OMP_PROC_BIND=true \
        perf stat -r 5 -e task-clock,cycles,instructions,cache-references,cache-misses \
        -- ./cg > /dev/null
done