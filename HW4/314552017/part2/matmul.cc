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
    int a_rows = n/size;
    if(rank < n % size){
       a_rows++;
    }
    *a_mat_ptr = new int[a_rows * m];
    *b_mat_ptr = new int[m * l];
    int sendcounts[size];
    int displs[size];
    if(rank == 0){
        displs[0] = 0;
        sendcounts[0] = (n/size) * m;
        if(0 < n % size){
            sendcounts[0] += m;
        }
        for(int i = 1 ; i < size; ++i){
            displs[i] = displs[i - 1] + sendcounts[i - 1];
            sendcounts[i] = (n/size) * m;
            if(i < n % size){
                sendcounts[i] += m;
            }
        }
        std::copy(b_mat, b_mat + m * l, *b_mat_ptr);
    }
    MPI_Scatterv(a_mat, sendcounts, displs, MPI_INT, *a_mat_ptr, a_rows * m, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(*b_mat_ptr, m * l, MPI_INT, 0, MPI_COMM_WORLD);
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
    int a_rows = (rank < n % size) ? ((n/size) + 1) : (n/size);
    int *local_out_mat = new int[a_rows * l];
    for(int i = 0; i < a_rows; ++i)
    {
        int j = 0;
        // Unrolling the loop for better performance
        // one a row, calculate 4 rows of b at the same time
        for(; j+3 < l; j+=4){
            int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
            for(int k = 0; k < m; ++k){
                int a = a_mat[i * m + k];
                sum0 += a * b_mat[j * m + k];
                sum1 += a * b_mat[(j + 1) * m + k];
                sum2 += a * b_mat[(j + 2) * m + k];
                sum3 += a * b_mat[(j + 3) * m + k];
            }
            local_out_mat[i * l + j] = sum0;
            local_out_mat[i * l + j + 1] = sum1;
            local_out_mat[i * l + j + 2] = sum2;
            local_out_mat[i * l + j + 3] = sum3;
        }
        // rest of the b rows.
        for(; j < l; ++j){
            int sum = 0;
            for(int k = 0; k < m; ++k){
                sum += a_mat[i * m + k] * b_mat[j * m + k];
            }
            local_out_mat[i * l + j] = sum;
        }
    }
    int recvcounts[size];
    int displs[size];
    if(rank == 0){
        displs[0] = 0;
        recvcounts[0] = (n/size) * l;
        if(n % size != 0){
            recvcounts[0] += l;
        }
        for(int i = 1 ; i < size; ++i){
            displs[i] = displs[i - 1] + recvcounts[i - 1];
            recvcounts[i] = (n/size) * l;
            if(i < n % size){
                recvcounts[i] += l;
            }
        }
    }
    MPI_Gatherv(local_out_mat, a_rows * l, MPI_INT, out_mat, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
    delete[] local_out_mat;
}

void destruct_matrices(int *a_mat, int *b_mat)
{
    /* TODO */
    delete[] a_mat;
    delete[] b_mat;
}
