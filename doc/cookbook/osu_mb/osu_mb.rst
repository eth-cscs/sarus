********************
OSU Micro benchmarks
********************

The `OSU Micro Benchmarks (OMB) <http://mvapich.cse.ohio-state.edu/benchmarks/>`_
are a widely used suite of benchmarks for measuring and evaluating the
performance of MPI operations for point-to-point, multi-pair, and collective
communications. These benchmarks are often used for comparing different MPI
implementations and the underlying network interconnect.

We use OMB to show that Sarus is able to provide the same native MPI high
performance to containerized applications when using the native MPICH hook.
As indicated in the documentation for the hook, the only conditions required are:

* The MPI installed in the container image must comply to the requirements of the
  `MPICH ABI Compatibility Initiative <http://www.mpich.org/abi/>`_.
* The application in the container image must be dynamically linked with the
  MPI libraries.

Test cases
==========

Latency
-------
The OSU point-to-point latency (``osu_latency``) test performs a ping-pong
communication between a sender and a receiver where the sender sends a message
and waits for the reply from the receiver. The messages are sent repeatedly for
a variety of data sizes in order to report the average one-way latency. This
test allows us to observe any possible overhead from enabling the MPI support
provided by Sarus.

All-to-all
----------
The ``osu_alltoall`` benchmark measures the min, max and the average latency of
the MPI_Alltoall blocking collective operation across N processes, for various
message lengths, over a large number of iterations. In the default version,
these benchmarks report the average latency for each message length up to 1MB.
We run this benchmark from a minimum of 2 nodes up to 128 nodes, increasing the node
count in powers of two.

Native application
==================
We compile the OSU micro benchmark suite version 5.3.2 natively using the Cray
Programming Environment and linking against the optimized Cray MPT 7.7.2
libraries.

Container images and Dockerfiles
================================
We built three container images with the OSU micro benchmark suite version 5.3.2,
in order to better demonstrate the effectiveness of the hook regardless of the
ABI-compatible MPI implementation present in the image:

1. MPICH 3.1.4 based on Debian 8.
   This image is available on Docker Hub at `ethcscs/osu-mb:5.3.2-mpich3.1.4 <https://hub.docker.com/r/ethcscs/osu-mb/tags/>`_.

.. code-block:: docker

   FROM debian:jessie

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

   RUN wget -q http://mvapich.cse.ohio-state.edu/download/mvapich/osu-micro-benchmarks-5.3.2.tar.gz \
       && tar xf osu-micro-benchmarks-5.3.2.tar.gz \
       && cd osu-micro-benchmarks-5.3.2 \
       && ./configure --prefix=/usr/local CC=$(which mpicc) CFLAGS=-O3 \
       && make \
       && make install \
       && cd .. \
       && rm -rf osu-micro-benchmarks-5.3.2 \
       && rm osu-micro-benchmarks-5.3.2.tar.gz

   WORKDIR /usr/local/libexec/osu-micro-benchmarks/mpi/pt2pt
   CMD ["mpiexec", "-n", "2", "-bind-to", "core", "./osu_bw"]

2. MVAPICH 2.2 based on Debian 8.
   This image is available on Docker Hub at ``ethcscs/osu-mb:5.3.2-mvapich2.2``:

.. code-block:: docker

   FROM debian:jessie

   RUN apt-get update \
       && apt-get install -y --no-install-recommends \
           bison \
           file \
           g++ \
           gcc \
           gfortran \
           libibverbs-dev \
           make \
           perl-modules \
           wget \
           realpath \
           strace \
       && rm -rf /var/lib/apt/lists/*

   # install MVAPICH2 v2.2 from source (this bundles OSU Micro-benchmarks v5.3.2)
   RUN wget -q -O mvapich2-2.2.tar.gz http://mvapich.cse.ohio-state.edu/download/mvapich/mv2/mvapich2-2.2.tar.gz \
       && tar -xf mvapich2-2.2.tar.gz \
       && cd mvapich2-2.2 \
       && ./configure  --prefix=/usr/local --disable-mcast --disable-xrc \
       && make -j4 \
       && make check \
       && make install \
       && ldconfig \
       && cd .. \
       && rm -rf mvapich2-2.2.tar.gz mvapich2-2.2

   WORKDIR /usr/local/libexec/osu-micro-benchmarks/mpi/pt2pt
   # we need to disable Cross Memory Attach (CMA), otherwise mpiexec fails
   ENV MV2_SMP_USE_CMA=0
   CMD ["mpiexec", "-n", "2", "-bind-to", "core", "./osu_bw"]

3. Intel MPI 2017 Update 1 based on Centos 7. Due to the license of the Intel MPI
   limiting redistribution of the software, the installation files (like
   configuration and license file) have to be present locally in the computer
   building the image.

.. code-block:: docker

   FROM centos:7

   #COPY etc-apt-sources.list /etc/apt/sources.list
   RUN yum install -y gcc \
           gcc-c++ \
           which \
           make \
           wget \
           strace \
           cpio

   # install Intel compiler + Intel MPI
   COPY intel_licence_file.lic /etc/intel_licence_file.lic
   COPY intel_installation_config_file /etc/intel_installation_config_file
   ADD parallel_studio_xe_2017_update1_cluster_edition_online.tgz .
   RUN cd parallel_studio_xe_2017_update1_cluster_edition_online \
       && ./install.sh --ignore-cpu -s /etc/intel_installation_config_file \
       && rm -rf parallel_studio_xe_2017_update1_cluster_edition_online
   ENV PATH /opt/intel/compilers_and_libraries_2017/linux/bin/intel64/:$PATH
   ENV PATH /opt/intel/compilers_and_libraries_2017/linux/mpi/intel64/bin:$PATH
   RUN echo "/opt/intel/compilers_and_libraries_2017/linux/mpi/intel64/lib" > /etc/ld.so.conf.d/intel_mpi.conf \
       && ldconfig

   # install OSU microbenchmarks
   RUN wget -q http://mvapich.cse.ohio-state.edu/download/mvapich/osu-micro-benchmarks-5.3.2.tar.gz \
       && tar xf osu-micro-benchmarks-5.3.2.tar.gz \
       && cd osu-micro-benchmarks-5.3.2 \
       && ./configure --prefix=/usr/local CC=$(which mpiicc) CFLAGS=-O3 LIBS="/opt/intel/compilers_and_libraries_2017.1.132/linux/compiler/lib/intel64_lin/libirc.a" \
       && make \
       && make install \
       && cd .. \
       && rm -rf osu-micro-benchmarks-5.3.2 \
       && rm osu-micro-benchmarks-5.3.2.tar.gz

   WORKDIR /usr/local/libexec/osu-micro-benchmarks/mpi/pt2pt
   CMD ["mpiexec", "-n", "2", "-bind-to", "core", "./osu_bw"]

Running the container
=====================
We run the container using the Slurm Workload Manager and Sarus.

Latency
-------

.. code-block:: bash

   srun -C gpu -N2 -t2 sarus run --mpi ethcscs/osu-mb:5.3.2-mpich3.1.4 /usr/local/libexec/osu-micro-benchmarks/mpi/pt2pt/osu_latency

   # OSU MPI Latency Test v5.3.2
   # Size          Latency (us)
   0                       1.11
   1                       1.11
   2                       1.09
   4                       1.09
   8                       1.09
   16                      1.10
   32                      1.09
   64                      1.10
   128                     1.11
   256                     1.12
   512                     1.15
   1024                    1.39
   2048                    1.67
   4096                    2.27
   8192                    4.21
   16384                   5.12
   32768                   6.73
   65536                  10.07
   131072                 16.69
   262144                 29.96
   524288                 56.45
   1048576               109.28
   2097152               216.29
   4194304               431.85

Since the Dockerfiles use the ``WORKDIR`` instruction to set a default working
directory, we can use that to simplify the terminal command:

.. code-block:: bash

   srun -C gpu -N2 -t2 sarus run --mpi ethcscs/osu-mb:5.3.2-mpich3.1.4 ./osu_latency

All-to-all
----------
.. code-block:: bash

   srun -C gpu -N2 -t2 sarus run --mpi ethcscs/osu-mb:5.3.2-mpich3.1.4 ../collective/osu_alltoall

   # OSU MPI All-to-All Personalized Exchange Latency Test v5.3.2
   # Size       Avg Latency(us)
   1                       5.46
   2                       5.27
   4                       5.22
   8                       5.21
   16                      5.18
   32                      5.18
   64                      5.17
   128                    11.35
   256                    11.64
   512                    11.72
   1024                   12.03
   2048                   12.87
   4096                   14.52
   8192                   15.77
   16384                  19.78
   32768                  28.89
   65536                  49.38
   131072                 96.64
   262144                183.23
   524288                363.35
   1048576               733.93

Results
=======

Latency
-------
Consider now the following Figure that compares the average and
standard deviation of the ``osu_latency`` test results for the four tested
configurations.
It can be observed that Sarus with the native MPI hook allows containers to
transparently access the accelerated networking hardware on Piz Daint and
achieve the same performance as the natively built test.

.. _fig-osu-latency-results:

.. figure:: plot_native_mpich_mvapich_intelmpi.*
   :scale: 100 %
   :alt: OSU Latency results

   Results of the OSU Latency benchmark for the native MPI and three different
   containers with ABI-compliant MPI libraries. The MPI in the container is
   replaced at runtime by the native MPICH MPI hook used by Sarus.

All-to-all
----------
We run the ``osu_alltoall`` benchmark only for two applications: native and
container with MPICH 3.1.4. We collect latency values for 1kB, 32kB, 65kB and
1MB message sizes, computing averages and standard deviation. The results are
displayed in the following Figure:

.. _fig-osu-alltoall-results:

.. figure:: alltoall-results.*
   :scale: 100 %
   :alt: OSU All-to-all results

   Results of the OSU All-to-all benchmark for the native MPI and MPICH 3.1.4
   container. The MPI in the container is replaced at runtime by the native MPICH
   MPI hook used by Sarus.

We observe that the results from the container are very close  to the native
results, for both average values and variability, across the node counts and
message sizes. The average value of the native benchmark for 1kB message size at
16 nodes is slightly higher than the one computed for the container benchmark.

It is worthy to note that the results this benchmark are heavily influenced by
the topology of the tested set of nodes, especially regarding their variabiliy.
This means that other tests using the same node counts may achieve significantly
different results. It also implies that results at different node counts are
only indicative and not directly relatable, since we did not allocate the same
set of nodes for all node counts.
