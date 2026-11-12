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
    x ^= x >>17;
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

    // TODO: init MPI
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
    if (world_rank > 0)
    {
        // TODO: handle workers
        MPI_Send(&count, 1, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD);
    }
    else if (world_rank == 0)
    {
        // TODO: main
        for(int i = 1 ; i < world_size; i++){
            long long int tmp = 0;
            MPI_Status status;
            MPI_Recv(&tmp, 1, MPI_LONG_LONG_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            count += tmp;
        }
    }

    if (world_rank == 0)
    {
        // TODO: process PI result

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
