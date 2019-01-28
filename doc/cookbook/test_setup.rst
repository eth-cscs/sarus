**********
Test Setup
**********

The examples and performance measurements reported in this document were carried
out on `Piz Daint <https://www.cscs.ch/computers/piz-daint/>`_, a hybrid Cray
XC50/XC40 system in production at the Swiss National Supercomputing Centre
(CSCS) in Lugano, Switzerland. The system compute nodes are connected by the
Cray Aries interconnect under a Dragonfly topology, notably providing users
access to hybrid CPU-GPU nodes. Hybrid nodes are equipped with an Intel Xeon
E5-2690v3 processor, 64 GB of RAM, and a single NVIDIA Tesla P100 GPU with 16 GB
of memory. The software environment on Piz Daint at the time of writing is the
Cray Linux Environment 6.0.UP07 (CLE 6.0) using Environment Modules to provide
access to compilers, tools, and applications. The default versions for the
NVIDIA CUDA and MPI software stacks are, respectively, CUDA version 9.1, and
Cray MPT version 7.7.2.

We install and configure Sarus on Piz Daint to use the native MPI and NVIDIA
Container Runtime hooks, and to mount the container images from a Lustre
parallel filesystem.

Performance measurements
========================
In tests comparing native and container performance numbers, each data point
presents the average and standard deviation of 50 runs, to produce statistically
relevant results, unless otherwise noted. For a given application, all
repetitions at each node count for both native and container execution were
performed on the same allocated set of nodes.
