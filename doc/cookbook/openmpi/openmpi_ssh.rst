.. _openmpi-ssh:

OpenMPI with SSH launcher
*************************

`OpenMPI <https://www.open-mpi.org/>`_ is an MPI implementation with widespread
adoption and support throughout the HPC community, and is featured in several
official Docker images as well.

The substantial differences with MPICH-based MPI implementations make the native
MPICH hook provided by Sarus ineffective on container images using OpenMPI.

This example will showcase how to run an OpenMPI container application by using
an SSH launcher and the SSH Hook.

Test case
=========
We use the "Hello world in C" program included with the OpenMPI codebase in the
`examples/hello_c.c
<https://github.com/open-mpi/ompi/blob/master/examples/hello_c.c>`_ file.
We do not measure performance with this test and we do not make comparisons with
a native implementation.

The program code at the time of writing follows for reference:

.. code-block:: cpp

   /*
    * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
    *                         University Research and Technology
    *                         Corporation.  All rights reserved.
    * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
    *
    * Sample MPI "hello world" application in C
    */

   #include <stdio.h>
   #include "mpi.h"

   int main(int argc, char* argv[])
   {
       int rank, size, len;
       char version[MPI_MAX_LIBRARY_VERSION_STRING];

       MPI_Init(&argc, &argv);
       MPI_Comm_rank(MPI_COMM_WORLD, &rank);
       MPI_Comm_size(MPI_COMM_WORLD, &size);
       MPI_Get_library_version(version, &len);
       printf("Hello, world, I am %d of %d, (%s, %d)\n",
              rank, size, version, len);
       MPI_Finalize();

       return 0;
   }

Container image and Dockerfile
==============================
We start from the official image for Debian 8 "Jessie", install the required build
tools, OpenMPI 3.1.3, and compile the example code using the MPI compiler.

.. code-block:: docker

   FROM debian:jessie

   RUN apt-get update \
       && apt-get install -y ca-certificates \
                             file \
                             g++ \
                             gcc \
                             gfortran \
                             make \
                             gdb \
                             strace \
                             realpath \
                             wget \
                             --no-install-recommends

   RUN wget -q https://download.open-mpi.org/release/open-mpi/v3.1/openmpi-3.1.3.tar.gz \
       && tar xf openmpi-3.1.3.tar.gz \
       && cd openmpi-3.1.3 \
       && ./configure --with-slurm=no --prefix=/usr/local \
       && make all install
   #./configure --with-slurm=no --prefix=/usr/local

   RUN ldconfig
   RUN cd /openmpi-3.1.3/examples && mpicc hello_c.c -o hello_c

Used OCI hooks
==============
* SSH hook
* SLURM global sync hook

Running the container
=====================
To run an OpenMPI program using the SSH hook, we need to manually provide a list
of hosts and explicitly launch ``mpirun`` only on one node of the allocation.
We can do so with the following commands:

.. code-block:: bash

   salloc -C gpu -N4 -t5
   sarus ssh-keygen
   srun hostname > $SCRATCH/hostfile
   srun sarus run --ssh \
        --mount=src=$SCRATCH,dst=$SCRATCH,type=bind \
        ethcscs/openmpi:3.1.3  \
        bash -c 'if [ $SLURM_PROCID -eq 0 ]; then mpirun --hostfile $SCRATCH/hostfile -npernode 1 /openmpi-3.1.3/examples/hello_c; else sleep infinity; fi'

   Warning: Permanently added '[nid02182]:15263,[148.187.40.151]:15263' (RSA) to the list of known hosts.
   Warning: Permanently added '[nid02180]:15263,[148.187.40.149]:15263' (RSA) to the list of known hosts.
   Warning: Permanently added '[nid02181]:15263,[148.187.40.150]:15263' (RSA) to the list of known hosts.
   Hello, world, I am 0 of 4, (Open MPI v3.1.3, package: Open MPI root@74cce493748b Distribution, ident: 3.1.3, repo rev: v3.1.3, Oct 29, 2018, 112)
   Hello, world, I am 3 of 4, (Open MPI v3.1.3, package: Open MPI root@74cce493748b Distribution, ident: 3.1.3, repo rev: v3.1.3, Oct 29, 2018, 112)
   Hello, world, I am 2 of 4, (Open MPI v3.1.3, package: Open MPI root@74cce493748b Distribution, ident: 3.1.3, repo rev: v3.1.3, Oct 29, 2018, 112)
   Hello, world, I am 1 of 4, (Open MPI v3.1.3, package: Open MPI root@74cce493748b Distribution, ident: 3.1.3, repo rev: v3.1.3, Oct 29, 2018, 112)
