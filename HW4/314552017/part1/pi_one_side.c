#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


static inline unsigned int xorshifht(unsigned int* stat){
    unsigned int x = *stat;
    x ^= x << 13;
    x ^= x >>17;
    x ^= x << 5;
    *stat = x;
    return x;
}


int fnz(long long int *schedule, long long int *oldschedule, int size)
{
    int diff = 0;

    for (int i = 0; i < size; i++)
        diff |= (schedule[i] != oldschedule[i]);

    if (diff)
    {
        int res = 0;
        for (int i = 0; i < size; i++)
        {
            res += (schedule[i] != 0);
            oldschedule[i] = schedule[i];
        }
        return (res == size);
    }
    return 0;
}

int main(int argc, char **argv)
{
    // --- DON'T TOUCH ---
    MPI_Init(&argc, &argv);
    double start_time = MPI_Wtime();
    double pi_result;
    long long int tosses = atoi(argv[1]);
    int world_rank, world_size;
    // ---

    MPI_Win win;

    // TODO: MPI init
    MPI_Comm_rank(MPI_COMM_WORLD,&world_rank);
    MPI_Comm_size(MPI_COMM_WORLD,&world_size);
    unsigned int state = (unsigned int )time(NULL) ^ (unsigned int ) (world_rank + 1) * 0x123456789;
    unsigned int state2 = (unsigned int )time(NULL) ^ (unsigned int ) (world_rank + 1) * 0x45678912;
    long long int count = 0;
    long long int tosses_in_this = tosses / (long long int)world_size;
    int remainder = tosses%(long long int)world_size;
    if(remainder != 0 && world_rank < remainder){
        tosses_in_this += 1;
    }
    for(long long int toss = 0 ; toss < tosses_in_this; toss++){
        double x = (double) xorshifht(&state) * (1.0 / 4294967295.0);
        double y = (double) xorshifht(&state2) * (1.0 / 4294967295.0);
        count += (x * x + y * y <= 1.0);
    }
    long long int *old = NULL;
    if (world_rank == 0)
    {
        old = malloc(sizeof(long long int) * (world_size-1));
        long long int *cur;
        MPI_Alloc_mem((long)(sizeof(long long int) * (world_size-1)), MPI_INFO_NULL, (void *)&cur);

        for(int i = 0 ; i < world_size-1; i++){
            old[i] = -1;
            cur[i] = 0;
        }

        MPI_Win_create(cur, (long)(sizeof(long long int) * (world_size-1)), sizeof(long long int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        
        int ready = 0;
        while(!ready){
            MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, win);
            ready = fnz(cur,old,world_size-1);
            MPI_Win_unlock(0, win);
        }
        MPI_Free_mem(cur);
    }
    else
    {
        // Workers
        MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
        MPI_Put(&count, 1, MPI_LONG_LONG_INT, 0, world_rank-1, 1, MPI_LONG_LONG_INT, win);
        MPI_Win_unlock(0, win);
    }

    MPI_Win_free(&win);
    
    if (world_rank == 0)
    {
        // TODO: handle PI result
        for(int i = 0 ; i< world_size-1;i++){
            count += old[i];
        }
        free(old);
        pi_result = (4.0 * count) / tosses;
        // --- DON'T TOUCH ---
        double end_time = MPI_Wtime();
        printf("%lf\n", pi_result);
        printf("MPI running time: %lf Seconds\n", end_time - start_time);
        // ---
    }

    MPI_Finalize();
    return 0;
}
