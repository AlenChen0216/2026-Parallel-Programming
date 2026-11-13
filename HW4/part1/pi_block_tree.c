#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>


unsigned int xorshifht(unsigned int* stat){
    unsigned int x = *stat;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *stat = x;
    return x;
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

    // TODO: MPI init
    MPI_Comm_rank(MPI_COMM_WORLD,&world_rank);
    MPI_Comm_size(MPI_COMM_WORLD,&world_size);
    unsigned int state = (unsigned int )time(NULL) ^ (unsigned int ) (world_rank + 1) * 0x123456789;
    long long int count = 0;
    long long int tosses_in_this = tosses / (long long int)world_size;
    int remainder = tosses%(long long int)world_size;
    if(remainder != 0 && world_rank < remainder){
        tosses_in_this += 1;
    }
    for(int toss = 0 ; toss < tosses_in_this; toss++){
        double x = (double) xorshifht(&state)/ UINT32_MAX;
        double y = (double) xorshifht(&state)/ UINT32_MAX;
        if(x*x + y*y <=1){
            count++;
        }
    }
    int step = 2;
    int pre_step = 1;
    int iter = 0 , tmp_size = world_size/2;
    while(tmp_size != 0){
        if(world_rank % tmp_size == 0 ) iter++;
        tmp_size >>= 1;
    }
    while(iter--){
        long long int tmp;
        MPI_Status status;
        if(world_rank % step == 0){
            MPI_Recv(&tmp,1,MPI_LONG_LONG_INT,(world_rank+ pre_step), 0, MPI_COMM_WORLD, &status);
            count += tmp;
        }else if(world_rank % step == pre_step){
            MPI_Send(&count,1,MPI_LONG_LONG_INT, (world_rank - pre_step), 0, MPI_COMM_WORLD);
        }
        pre_step <<= 1;
        step <<= 1;
    }
    // TODO: binary tree redunction
    
    if (world_rank == 0)
    {
        // TODO: PI result
        pi_result = pi_result = (4.0 * count) / tosses;
        // --- DON'T TOUCH ---
        double end_time = MPI_Wtime();
        printf("%lf\n", pi_result);
        printf("MPI running time: %lf Seconds\n", end_time - start_time);
        // ---
    }

    MPI_Finalize();
    return 0;
}
