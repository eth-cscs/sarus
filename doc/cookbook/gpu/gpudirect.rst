*********************
NVIDIA GPUDirect RDMA
*********************

`GPUDirect <https://developer.nvidia.com/gpudirect>`_ is a technology
that enables direct RDMA to and from GPU memory. This means that multiple GPUs
can directly read and write CUDA host and device memory, without resorting to
the use of host memory or the CPU, resulting in significant data transfer
performance improvements.

We will show here that Sarus is able to leverage the GPUDirect technology.

Test case
=========
This sample  
:download:`C++ code</cookbook/dockerfiles/gpudirect/all_gather/all_gather.cpp>` 
performs an MPI_Allgather operation using CUDA device memory and GPUDirect. If
the operation is carried out successfully, the program prints a success message
to standard output.

Running the container
=====================
Before running this code with Sarus, two environment variables must be set: 
``MPICH_RDMA_ENABLED_CUDA`` and ``LD_PRELOAD``

.. code-block:: bash

    MPICH_RDMA_ENABLED_CUDA: allows the MPI application to pass GPU
    pointers directly to point-to-point and collective communication functions,
    as well as blocking collective communication functions.

    LD_PRELOAD: allows to load the specified cuda library from the
    compute node before all others.

This can be done by passing a string command to bash:

.. code-block:: bash

   srun -C gpu -N4 -t2 sarus run --mpi \
       ethcscs/mpich:ub1804_cuda92_mpi314_gpudirect-all_gather
       bash -c 'MPICH_RDMA_ENABLED_CUDA=1 LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libcuda.so ./all_gather'

A successful output looks like:

.. code-block:: bash

   Success!

Running the native application
==============================

.. code-block:: bash

   export MPICH_RDMA_ENABLED_CUDA=1
   srun -C gpu -N4 -t2 ./all_gather

A successful output looks like:

.. code-block:: bash

   Success!

Container image and Dockerfile
==============================

The container image (based on cuda/9.2 and mpich/3.1.4) used for this test case
can be pulled from CSCS `DockerHub
<https://hub.docker.com/r/ethcscs/mpich/tags>`_ or be rebuilt with this 
:download:`Dockerfile </cookbook/dockerfiles/gpudirect/all_gather/Dockerfile>`:

.. literalinclude:: /cookbook/dockerfiles/gpudirect/all_gather/Dockerfile
   :language: docker
   :linenos:

Required OCI hooks
==================
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)
