***********
CUDA N-body
***********

A fast n-body simulation is included as part of the `CUDA Software Development
Kit samples <https://developer.nvidia.com/cuda-code-samples>`_. The CUDA n-body
sample code simulates the gravitational interaction and motion of a group of
bodies.  The code is written with CUDA and C and can make efficient use of
multiple GPUs to calculate all-pairs gravitational interactions. More details
of the implementation can be found in this article by Lars Nyland et al.:
`Fast N-Body Simulation with CUDA
<https://developer.download.nvidia.com/compute/cuda/1.1-Beta/x86_website/projects/nbody/doc/nbody_gems3_ch31.pdf>`_.

We use this sample code to show that Sarus is able to leverage the NVIDIA
Container Runtime hook in order to provide containers with native performance
from NVIDIA GPUs present in the host system.

Test Case
=========
For this test case, we run the code with :math:`n=200,000` bodies using
double-precision floating-point arithmetic on 1 Piz Daint compute node,
featuring a single Tesla P100 GPU.

Running the container
=====================
We run the container using the Slurm Workload Manager and Sarus:

.. code-block:: bash

    srun -Cgpu -N1 -t1 \
        sarus run ethcscs/cudasamples:9.2 \
        /usr/local/cuda/samples/bin/x86_64/linux/release/nbody \
        -benchmark -fp64 -numbodies=200000

A typical output will look like:

.. code-block:: bash

    Run "nbody -benchmark [-numbodies=<numBodies>]" to measure performance.
	-fullscreen       (run n-body simulation in fullscreen mode)
	-fp64             (use double precision floating point values for simulation)
	-hostmem          (stores simulation data in host memory)
	-benchmark        (run benchmark to measure performance)
	-numbodies=<N>    (number of bodies (>= 1) to run in simulation)
	-device=<d>       (where d=0,1,2.... for the CUDA device to use)
	-numdevices=<i>   (where i=(number of CUDA devices > 0) to use for simulation)
	-compare          (compares simulation results running once on the default 
                      GPU and once on the CPU)
	-cpu              (run n-body simulation on the CPU)
	-tipsy=<file.bin> (load a tipsy model file for simulation)

    NOTE: The CUDA Samples are not meant for performance measurements. 
    Results may vary when GPU Boost is enabled.

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

Running the native Application
==============================
We compile and run the same code on Piz Daint using a similar Cuda Toolkit 
version (cudatoolkit/9.2).

Container image and Dockerfile
==============================

The container image (based on Nvidia cuda/9.2) used for this test case
can be pulled from CSCS `DockerHub
<https://hub.docker.com/r/ethcscs/cudasamples/tags>`_ or be rebuilt with this 
:download:`Dockerfile </cookbook/dockerfiles/nbody/Dockerfile.cudasamples>`:

.. literalinclude:: /cookbook/dockerfiles/nbody/Dockerfile.cudasamples
   :language: docker
   :linenos:

Required OCI hooks
==================
* NVIDIA Container Runtime hook


Benchmarking results
====================
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
