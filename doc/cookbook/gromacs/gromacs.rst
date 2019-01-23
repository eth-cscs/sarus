*******
GROMACS
*******

`GROMACS <http://www.gromacs.org/>`_ is a molecular dynamics package with an
extensive array of modeling, simulation and analysis capabilities. While
primarily developed for the simulation of biochemical molecules, its broad
adoption includes reaserch fields such as non-biological chemistry, metadynamics
and mesoscale physics. One of the key aspects characterizing GROMACS is the
strong focus on high performance and resource efficiency, making use of
state-of-the-art algorithms and optimized low-level programming techniques for
CPUs and GPUs.

Test case
=========
As test case, we select the 3M atom system from the `HECBioSim benchmark suite
for Molecular Dynamics <http://www.hecbiosim.ac.uk/benchmarks>`_. The system
represents a pair of hEGFR tetramers of 1IVO and 1NQL structures, and consists
of 2,997,924 atoms in total.

The simulation was carried out using single precision, 1 MPI process per node
and 12 OpenMP threads per MPI process. We performed runs from a minimum of 4
nodes up to 64 nodes, increasing the node count in powers of two.

Native application
==================
As the native application we use GROMACS release 2018.3, compiled with FFTW
3.3.6, CUDA 9.1 and AVX2-256 SIMD instructions. The application is built by CSCS
staff and provided to the users of Piz Daint through an environment module.

Container image and Dockerfile
==============================
For the container application, we start from the official NVIDIA CUDA 9.1 image,
adding MPICH 3.1.4 and CMake 3.12. GROMACS 2018.3 is then compiled with support
for single precision floating point arithmetic, GPU acceleration, and AVX-256
SIMD instructions. The application is also instructed to build FFTW on its own,
as advised by its developers, and proceeds to build FFTW 3.3.8 as a dependency.

.. code-block:: docker

   FROM nvidia/cuda:9.1-devel-ubuntu16.04

   RUN apt-get update \
   && apt-get install -y --no-install-recommends \
           wget \
           gfortran \
           zlib1g-dev \
           libopenblas-dev \
   && rm -rf /var/lib/apt/lists/*

   # Install MPICH
   RUN wget -q http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz \
   && tar xf mpich-3.1.4.tar.gz \
   && cd mpich-3.1.4 \
   && ./configure --disable-fortran --enable-fast=all,O3 --prefix=/usr \
   && make -j$(nproc) \
   && make install \
   && ldconfig

   # Install CMake
   # The package manager for Ubuntu 16.04 would install version 3.5.1
   # Here we will manually install a more recent version
   RUN mkdir /usr/local/cmake \
   && cd /usr/local/cmake \
   && wget -q https://cmake.org/files/v3.12/cmake-3.12.4-Linux-x86_64.tar.gz \
   && tar -xzf cmake-3.12.4-Linux-x86_64.tar.gz \
   && mv cmake-3.12.4-Linux-x86_64 3.12.4 \
   && rm cmake-3.12.4-Linux-x86_64.tar.gz \
   && cd /

   ENV PATH=/usr/local/cmake/3.12.4/bin/:${PATH}

   # Install GROMACS
   RUN wget -q http://ftp.gromacs.org/pub/gromacs/gromacs-2018.3.tar.gz \
   && tar xf gromacs-2018.3.tar.gz \
   && cd gromacs-2018.3 \
   && mkdir build && cd build \
   && cmake -DCMAKE_BUILD_TYPE=Release  \
           -DGMX_BUILD_OWN_FFTW=ON -DREGRESSIONTEST_DOWNLOAD=ON \
           -DGMX_MPI=on -DGMX_GPU=on -DGMX_SIMD=AVX2_256 -DGMX_BUILD_MDRUN_ONLY=on \
            .. \
   && make -j$(nproc) \
   && make check \
   && make install \
   && cd ../.. \
   && rm -r gromacs-2018.3 gromacs-2018.3.tar.gz

Running the container
=====================
Assuming that the ``benchmark.tpr`` input data is present in a directory which
Sarus is configured to automatically mount inside the container (here referred
by the arbitrary variable ``$INPUT``), we can run the container application as
follows:

.. code-block:: bash

   srun -C gpu -N16 -t200 srun sarus run --mpi \
       ethcscs/gromacs:2018.3-cuda9.1-mpich3.1.4  \
       /usr/local/gromacs/bin/mdrun_mpi -s ${INPUT}/benchmark.tpr -ntomp 12

If the system administrator did not configure Sarus to mount the input data
location during container setup, we can use the ``--mount`` option:

.. code-block:: bash

   srun -C gpu -N16 -t200 sarus run --mpi \
       --mount=type=bind,src=<path-to-input-directory>,dst=/gromacs-data \
       ethcscs/gromacs:2018.3-cuda9.1-mpich3.1.4 \
       /usr/local/gromacs/bin/mdrun_mpi -s /gromacs-data/benchmark.tpr -ntomp 12

Results
=======
We measure wall clock time (in seconds) and performance (in ns/day) as reported
by the application logs. The speedup values are computed using the wall clock
time averages for each data point, taking the native execution time at 4 nodes
as baseline. The results of our experiments are illustrated in the following
figure:

.. _fig-gromacs-results:

.. figure:: gromacs-results.*
   :scale: 100%
   :alt: GROMACS results

   Comparison of wall clock execution time, performance, and speedup between native
   and Sarus-deployed container versions of GROMACS on Piz Daint.

We observe the container application being up to 6% faster than the native
implementation, with a small but consistent performance advantage and comparable
standard deviations across the different node counts.
