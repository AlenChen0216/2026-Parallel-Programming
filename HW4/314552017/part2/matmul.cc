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
    for(int j_block = 0; j_block < l; j_block += 64){
        int j_block_end = std::min(j_block + 64, l);
        int i = 0;
        for(; i + 1 < a_rows; i += 2)
        {
            int j = j_block;
            for(; j + 3 < j_block_end; j += 4){
                int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
                int sum4 = 0, sum5 = 0, sum6 = 0, sum7 = 0;
                for(int k = 0; k < m; ++k){
                    int a0 = a_mat[i * m + k], a1 = a_mat[(i + 1) * m + k];
                    int b0 = b_mat[j * m + k], b1 = b_mat[(j + 1) * m + k];
                    int b2 = b_mat[(j + 2) * m + k], b3 = b_mat[(j + 3) * m + k];
                    sum0 += a0 * b0; sum1 += a0 * b1;
                    sum2 += a0 * b2; sum3 += a0 * b3;
                    sum4 += a1 * b0; sum5 += a1 * b1;
                    sum6 += a1 * b2; sum7 += a1 * b3;
                }
                local_out_mat[i * l + j] = sum0;
                local_out_mat[i * l + j + 1] = sum1;
                local_out_mat[i * l + j + 2] = sum2;
                local_out_mat[i * l + j + 3] = sum3;
                local_out_mat[(i + 1) * l + j] = sum4;
                local_out_mat[(i + 1) * l + j + 1] = sum5;
                local_out_mat[(i + 1) * l + j + 2] = sum6;
                local_out_mat[(i + 1) * l + j + 3] = sum7;
            }
            for(; j < j_block_end; ++j){
                int sum0 = 0, sum1 = 0;
                for(int k = 0; k < m; ++k){
                    int b = b_mat[j * m + k];
                    sum0 += a_mat[i * m + k] * b;
                    sum1 += a_mat[(i + 1) * m + k] * b;
                }
                local_out_mat[i * l + j] = sum0;
                local_out_mat[(i + 1) * l + j] = sum1;
            }
        }
        for(; i < a_rows; ++i)
        {
            int j = j_block;
            // Unrolling the loop for better performance
            // one a row, calculate 4 rows of b at the same time
            for(; j+3 < j_block_end; j+=4){
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
            for(; j < j_block_end; ++j){
                int sum = 0;
                for(int k = 0; k < m; ++k){
                    sum += a_mat[i * m + k] * b_mat[j * m + k];
                }
                local_out_mat[i * l + j] = sum;
            }
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
