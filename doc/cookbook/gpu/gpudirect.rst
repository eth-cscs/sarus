**************
GPUDirect RDMA
**************

`GPUDirect <https://developer.nvidia.com/gpudirect>`_ is a technology available
in datacenter-class NVIDIA GPUs which enables direct RDMA to and from GPU
memory. This means that multiple GPUs, third party network adapters, solid-state
drives (SSDs) and other devices can directly read and write CUDA host and device
memory, without resorting to the use of host memory or the CPU, resulting in
significant data transfer performance improvements.

This example will showcase how GPUDirect can be leveraged using Sarus on a
Cray XC50 system.

Test case
=========
We write a small C++ program to perform an MPI_Allgather operation using
CUDA device memory and GPUDirect. If the operation is carried out successfully,
the program prints a success message to stdout.

The program code follows:

.. code-block:: cpp

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

       // Preform Allgather using device buffer
       MPI_Allgather(d_rank, 1, MPI_INT, d_buff, 1, MPI_INT, MPI_COMM_WORLD);

       // Check that the GPU buffer is correct
       cudaMemcpy(h_buff, d_buff, bytes, cudaMemcpyDeviceToHost);
       for(int i=0; i<size; i++){
           if(h_buff[i] != i) {
               printf ("Alltoall Failed!\n");
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

Container image and Dockerfile
==============================
We start from the official NVIDIA CUDA 8.0 image, install the required build
tools, MPICH 3.1.4, upload the ``all_gather.cpp`` program source into the image, and
compile the code using the MPI compiler.

.. code-block:: docker

   FROM nvidia/cuda:8.0-devel

   RUN apt-get update \
       && apt-get install -y file \
                             g++ \
                             gcc \
                             gfortran \
                             make \
                             gdb \
                             strace \
                             realpath \
                             wget \
                             --no-install-recommends

   RUN wget -q http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz \
       && tar xf mpich-3.1.4.tar.gz \
       && cd mpich-3.1.4 \
       && ./configure --disable-fortran --enable-fast=all,O3 --prefix=/usr \
       && make -j$(nproc) \
       && make install \
       && ldconfig

   COPY all_gather.cpp /opt/mpi_gpudirect/all_gather.cpp
   WORKDIR /opt/mpi_gpudirect
   RUN mpicxx -g all_gather.cpp -o all_gather -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart

Used OCI hooks
==============
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)

Running the container
=====================
To run a GPUDirect program with Sarus on a Cray XC50, we need to set two
environment variables in the container: ``MPICH_RDMA_ENABLED_CUDA=1`` and ``LD_PRELOAD``
targeting the CUDA shared library. We can do so by passing a string command
to Bash:

.. code-block:: bash

   srun -C gpu -N4 -t2 sarus run --mpi \
       ethcscs/dockerfiles:mpi_gpudirect-all_gather \
       bash -c 'MPICH_RDMA_ENABLED_CUDA=1 LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libcuda.so ./all_gather'

   Success!
