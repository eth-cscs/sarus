********************
OSU Micro benchmarks
********************

The OSU Micro Benchmarks `(OMB) <http://mvapich.cse.ohio-state.edu/benchmarks/>`_
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
The ``osu_latency`` benchmark measures the min, max and the average latency of
a ping-pong communication between a sender and a receiver where the sender
sends a message and waits for the reply from the receiver. The messages are
sent repeatedly for a variety of data sizes in order to report the average
one-way latency. This test allows us to observe any possible overhead from
enabling the MPI support provided by Sarus.

All-to-all
----------
The ``osu_alltoall`` benchmark measures the min, max and the average latency of
the MPI_Alltoall blocking collective operation across N processes, for various
message lengths, over a large number of iterations. In the default version,
this benchmark report the average latency for each message length up to 1MB.
We run this benchmark from a minimum of 2 nodes up to 128 nodes, increasing the
node count in powers of two.

Running the container
=====================
We run the container using the Slurm Workload Manager and Sarus.

Latency
-------

.. code-block:: bash

   sarus pull ethcscs/mvapich:ub1804_cuda92_mpi22_osu
   srun -C gpu -N2 -t2 \
    sarus run --mpi ethcscs/mvapich:ub1804_cuda92_mpi22_osu \
    /usr/local/libexec/osu-micro-benchmarks/mpi/pt2pt/osu_latency

A typical output looks like:

.. code-block:: bash

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

   srun -C gpu -N2 -t2 \
    sarus run --mpi ethcscs/osu-mb:5.3.2-mpich3.1.4 \
    ./osu_latency

All-to-all
----------
.. code-block:: bash

   srun -C gpu -N2 -t2 \
    sarus run --mpi ethcscs/osu-mb:5.3.2-mpich3.1.4 \
    ../collective/osu_alltoall

A typical outpout looks like:

.. code-block:: bash

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


Running the native application
==============================
We compile the OSU micro benchmark suite natively using the Cray Programming
Environment (PrgEnv-cray) and linking against the optimized Cray MPI
(cray-mpich) libraries.

Container images and Dockerfiles
================================
We built the OSU benchmarks on top of several images containing MPI, in order to
demonstrate the effectiveness of the MPI hook regardless of the ABI-compatible
MPI implementation present in the images:

MPICH
-----
The container image ``ethcscs/mpich:ub1804_cuda92_mpi314_osu`` (based on
mpich/3.1.4) used for this test case can be pulled from CSCS `DockerHub
<https://hub.docker.com/r/ethcscs/mpich/tags>`_ or be rebuilt with this
:download:`Dockerfile
</cookbook/dockerfiles/mpich/Dockerfile.ubuntu1804+cuda92+mpich314+osu>`.

MVAPICH
-------
The container image ``ethcscs/mvapich:ub1804_cuda92_mpi22_osu`` (based on
mvapich/2.2) used for this test case can be pulled from CSCS `DockerHub
<https://hub.docker.com/r/ethcscs/mvapich/tags>`_ or be rebuilt with this
:download:`Dockerfile
</cookbook/dockerfiles/mvapich/Dockerfile.ubuntu1804lts+cuda92+mvapich22+osu>`.
On the Cray, the supported Cray MPICH ABI is 12.0 (mvapich>2.2 requires
ABI/12.1 hence is not currently supported).

OpenMPI
-------
As OpenMPI is not part of the MPICH ABI Compatibility Initiative, ``sarus run
--mpi`` with OpenMPI is not supported. Documentation can be found on this
dedicated page: :ref:`openmpi-ssh`.

Intel MPI
---------
Because the Intel MPI license limits general redistribution of the software,
we do not share the Docker image ``ethcscs/intelmpi`` used for this test case.
Provided the Intel installation files (such as archive and license file) are
available locally on your computer, you could build your own image with this
example
:download:`Dockerfile </cookbook/dockerfiles/intelmpi/Dockerfile.intel2017>`.

Required OCI hooks
==================
* Native MPI hook (MPICH-based)

Benchmarking results
====================

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

It is worthy to note that the results of this benchmark are heavily influenced
by the topology of the tested set of nodes, especially regarding their
variabiliy. This means that other tests using the same node counts may achieve
significantly different results. It also implies that results at different node
counts are only indicative and not directly relatable, since we did not
allocate the same set of nodes for all node counts.
