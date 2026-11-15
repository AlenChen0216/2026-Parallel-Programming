#include <mpi.h>
#include <algorithm>

void construct_matrices(
    int n, int m, int l, const int *a_mat, const int *b_mat, int **a_mat_ptr, int **b_mat_ptr)
{
    /* TODO: The data is stored in a_mat and b_mat.
     * You need to allocate memory for a_mat_ptr and b_mat_ptr,
     * and copy the data from a_mat and b_mat to a_mat_ptr and b_mat_ptr, respectively.
     * You can use any size and layout you want if they provide better performance.
     * Unambitiously copying the data is also acceptable.
     *
     * The matrix multiplication will be performed on a_mat_ptr and b_mat_ptr.
     */

     // use scatter to distribute the data to all processes.
     // need to consider padding.
     int size,rank;
     MPI_Comm_size(MPI_COMM_WORLD, &size);
     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
     if(rank == size - 1){
        *a_mat_ptr = new int[((n/size) + (n % size)) * m];
     }else{
        *a_mat_ptr = new int[(n/size) * m];
     }
     *b_mat_ptr = new int[m * l];

     if(rank == 0){
        MPI_Request a_requests[size - 1];
        MPI_Request b_requests[size - 1];
        for(int i = 1 ; i< size ; i++){
            if(i == size - 1){
                MPI_Isend(a_mat + i * (n/size) * m, ((n/size) + (n % size)) * m, MPI_INT, i, 0, MPI_COMM_WORLD, &a_requests[i - 1]);
            }else{
                MPI_Isend(a_mat + i * (n/size) * m, (n/size) * m, MPI_INT, i, 0, MPI_COMM_WORLD, &a_requests[i - 1]);
            }
            MPI_Isend(b_mat, m * l, MPI_INT, i, 0, MPI_COMM_WORLD, &b_requests[i - 1]);
        }
        std::copy(a_mat, a_mat + (n/size) * m, *a_mat_ptr);
        std::copy(b_mat, b_mat + m * l, *b_mat_ptr);
        MPI_Waitall(size - 1, a_requests, MPI_STATUSES_IGNORE);
        MPI_Waitall(size - 1, b_requests, MPI_STATUSES_IGNORE);
        
     }else{
        if(rank == size - 1){
            MPI_Recv(*a_mat_ptr, ((n/size) + (n % size)) * m, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }else{
            MPI_Recv(*a_mat_ptr, (n/size) * m, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        MPI_Recv(*b_mat_ptr, m * l, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     }
}

void matrix_multiply(
    const int n, const int m, const int l, const int *a_mat, const int *b_mat, int *out_mat)
{
    /* TODO: Perform matrix multiplication on a_mat and b_mat. Which are the matrices you've
     * constructed. The result should be stored in out_mat, which is a continuous memory placing n *
     * l elements of int. You need to make sure rank 0 receives the result.
     */
     int size,rank;
     MPI_Comm_size(MPI_COMM_WORLD, &size);
     MPI_Comm_rank(MPI_COMM_WORLD, &rank);

     int a_rows = (rank == size - 1) ? ((n/size) + (n % size)) : (n/size);
     int *local_out_mat = new int[a_rows * l];

     for(int i = 0; i < a_rows; ++i)
     {
         for(int j = 0; j < l; ++j)
         {
             local_out_mat[i * l + j] = 0;
             for(int k = 0; k < m; ++k)
             {
                 local_out_mat[i * l + j] += a_mat[i * m + k] * b_mat[j * m + k];
             }
         }
     }
    if(rank == 0)
    {
        std::copy(local_out_mat, local_out_mat + a_rows * l, out_mat);
        MPI_Request requests[size - 1];
        for(int i = 1; i < size; ++i)
        {
            int rows_to_receive = (i == size - 1) ? ((n/size) + (n % size)) : (n/size);
            MPI_Irecv(out_mat + i * (n/size) * l, rows_to_receive * l, MPI_INT, i, 0, MPI_COMM_WORLD, &requests[i - 1]);
        }
        MPI_Waitall(size - 1, requests, MPI_STATUSES_IGNORE);
    }
    else
    {
        MPI_Send(local_out_mat, a_rows * l, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    delete[] local_out_mat;
}

void destruct_matrices(int *a_mat, int *b_mat)
{
    /* TODO */
    delete[] a_mat;
    delete[] b_mat;
}
