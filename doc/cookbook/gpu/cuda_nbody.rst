***********
CUDA N-body
***********

A fast n-body simulation is included as part of the `CUDA Software Development
Kit samples <https://developer.nvidia.com/cuda-code-samples>`_. The n-body
sample simulates the gravitational interaction and motion of a group of bodies.
The application is completely implemented in CUDA C and can make efficient use
of multiple GPUs to calculate all-pairs gravitational interactions. More details
of the implementation can be found in this article by Lars Nyland et al.:
`Fast N-Body Simulation with CUDA
<https://developer.download.nvidia.com/compute/cuda/1.1-Beta/x86_website/projects/nbody/doc/nbody_gems3_ch31.pdf>`_.

We use this sample code to show that Sarus is able to leverage the NVIDIA
Container Runtime hook in order to provide containers with native performance
from NVIDIA GPUs present in the host system.

Test Case
=========
As test case, we use the n-body benchmark to simulate :math:`n=200,000` bodies
using double-precision floating-point arithmetic. The benchmark was run on a
single-node from Piz Daint, featuring a single Tesla P100 GPU.

Native Application
==================
We compile the sample source code using the environment
module provided on Piz Daint for the CUDA Toolkit version 9.2.

Container image and Dockerfile
==============================
We created a Docker image by starting from the official Docker Hub image
provided by NVIDIA for CUDA 9.2, installing the CUDA SDK samples source code
through the system package manager, and compiling some of the samples with the
NVIDIA CUDA C compiler. This image is available on Docker Hub at
`ethcscs/cudasamples:9.2 <https://hub.docker.com/r/ethcscs/cudasamples/tags/>`_.

.. code-block:: docker

   FROM nvidia/cuda:9.2-devel

   RUN apt-get update && apt-get install -y --no-install-recommends \
           cuda-samples-$CUDA_PKG_VERSION && \
       rm -rf /var/lib/apt/lists/*

   RUN (cd /usr/local/cuda/samples/1_Utilities/bandwidthTest && make)
   RUN (cd /usr/local/cuda/samples/1_Utilities/deviceQuery && make)
   RUN (cd /usr/local/cuda/samples/1_Utilities/deviceQueryDrv && make)
   RUN (cd /usr/local/cuda/samples/1_Utilities/p2pBandwidthLatencyTest && make)
   RUN (cd /usr/local/cuda/samples/1_Utilities/topologyQuery && make)
   RUN (cd /usr/local/cuda/samples/5_Simulations/nbody && make)

   CMD ["/usr/local/cuda/samples/1_Utilities/deviceQuery/deviceQuery"]

Used OCI hooks
==============
* NVIDIA Container Runtime hook

Running the container
=====================
We run the container using the Slurm Workload Manager and Sarus:

.. code-block:: bash

    srun -C gpu -N1 -t1 sarus run ethcscs/cudasamples:9.2 /usr/local/cuda/samples/bin/x86_64/linux/release/nbody -benchmark -fp64 -numbodies=200000

    Run "nbody -benchmark [-numbodies=<numBodies>]" to measure performance.
	-fullscreen       (run n-body simulation in fullscreen mode)
	-fp64             (use double precision floating point values for simulation)
	-hostmem          (stores simulation data in host memory)
	-benchmark        (run benchmark to measure performance)
	-numbodies=<N>    (number of bodies (>= 1) to run in simulation)
	-device=<d>       (where d=0,1,2.... for the CUDA device to use)
	-numdevices=<i>   (where i=(number of CUDA devices > 0) to use for simulation)
	-compare          (compares simulation results running once on the default GPU and once on the CPU)
	-cpu              (run n-body simulation on the CPU)
	-tipsy=<file.bin> (load a tipsy model file for simulation)

    NOTE: The CUDA Samples are not meant for performance measurements. Results may vary when GPU Boost is enabled.

    > Windowed mode
    > Simulation data stored in video memory
    > Double precision floating point simulation
    > 1 Devices used for simulation
    GPU Device 0: "Tesla P100-PCIE-16GB" with compute capability 6.0

    > Compute 6.0 CUDA device: [Tesla P100-PCIE-16GB]
    Warning: "number of bodies" specified 200000 is not a multiple of 256.
    Rounding up to the nearest multiple: 200192.
    200192 bodies, total time for 10 iterations: 3927.009 ms
    = 102.054 billion interactions per second
    = 3061.631 double-precision GFLOP/s at 30 flops per interaction

Results
=======
We report the gigaflops per second performance attained by the two applications
in the following table:

+-----------+------------+----------------+
|           | Average    | Std. deviation |
+===========+============+================+
| Native    | 3059.34    | 5.30           |
+-----------+------------+----------------+
| Container | 3058.91    | 6.29           |
+-----------+------------+----------------+

The results show that containers deployed with Sarus
and the NVIDIA Container Runtime hook can achieve the same performance of the
natively built CUDA application, both in terms of average value and variability.
