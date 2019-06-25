#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <mpi.h>

int main( int argc, char** argv )
{
    MPI_Init (&argc, &argv);

    // Ensure that RDMA ENABLED CUDA is set correctly
    int direct;
    direct = getenv("MPICH_RDMA_ENABLED_CUDA")==NULL?0:atoi(getenv ("MPICH_RDMA_ENABLED_CUDA"));
    if(direct != 1){
        printf ("MPICH_RDMA_ENABLED_CUDA not enabled!\n");
        exit (EXIT_FAILURE);
    }

    // Get MPI rank and size
    int rank, size;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    // Allocate host and device buffers and copy rank value to GPU
    int *h_buff = NULL;
    int *d_rank = NULL;
    int *d_buff = NULL;
    size_t bytes = size*sizeof(int);
    h_buff = (int*)malloc(bytes);
    cudaMalloc(&d_buff, bytes);
    cudaMalloc(&d_rank, sizeof(int));
    cudaMemcpy(d_rank, &rank, sizeof(int), cudaMemcpyHostToDevice);

    // calls Allgather using device buffer
    MPI_Allgather(d_rank, 1, MPI_INT, d_buff, 1, MPI_INT, MPI_COMM_WORLD);

    // Check that the GPU buffer is correct
    cudaMemcpy(h_buff, d_buff, bytes, cudaMemcpyDeviceToHost);
    for(int i=0; i<size; i++){
        if(h_buff[i] != i) {
            printf ("Test Failed!\n");
            exit (EXIT_FAILURE);
        }
    }
    if(rank==0)
        printf("Success!\n");

    // Clean up
    free(h_buff);
    cudaFree(d_buff);
    cudaFree(d_rank);
    MPI_Finalize();

    return 0;
}
