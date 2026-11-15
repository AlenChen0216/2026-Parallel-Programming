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
    if (world_rank > 0)
    {
        // TODO: MPI workers
        MPI_Send(&count, 1, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD);
    }
    else if (world_rank == 0)
    {
        // TODO: non-blocking MPI communication.
        // Use MPI_Irecv, MPI_Wait or MPI_Waitall.

        MPI_Request requests[world_size-1];
        MPI_Status status[world_size-1];
        long long int data[world_size-1];
        for(int i = 1 ; i < world_size; i++){
            MPI_Irecv(&data[i-1], 1, MPI_LONG_LONG_INT, i, 0, MPI_COMM_WORLD, &requests[i-1]);
        }
        
        MPI_Waitall(world_size-1,requests,status);
        for(int i = 0 ; i < world_size-1; i++){
            count += data[i];
        }
    }

    if (world_rank == 0)
    {
        // TODO: PI result
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
