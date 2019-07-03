*******
GROMACS
*******

`GROMACS <https://user.cscs.ch/computing/applications/gromacs/>`_ is a
molecular dynamics package with an extensive array of modeling, simulation and
analysis capabilities. While primarily developed for the simulation of
biochemical molecules, its broad adoption includes reaserch fields such as
non-biological chemistry, metadynamics and mesoscale physics. One of the key
aspects characterizing GROMACS is the strong focus on high performance and
resource efficiency, making use of state-of-the-art algorithms and optimized
low-level programming techniques for CPUs and GPUs.

Test case
=========
As test case, we select the 3M atom system from the `HECBioSim
<http://www.hecbiosim.ac.uk/benchmarks>`_ benchmark suite for Molecular
Dynamics: a pair of ``hEGFR tetramers of 1IVO and 1NQL``:

.. code-block:: bash

    * Total number of atoms = 2,997,924
    * Protein atoms = 86,996  Lipid atoms = 867,784  Water atoms = 2,041,230  Ions = 1,914

The simulation is carried out using single precision, 1 MPI process per node
and 12 OpenMP threads per MPI process. We measured runtimes for 4, 8, 16, 32
and 64 compute nodes. The input file to download for the test case is
`3000k-atoms/benchmark.tpr
<http://www.hecbiosim.ac.uk/media/jdownloads/Software/gromacs.tar.gz>`_.

Running the container
=====================
Assuming that the ``benchmark.tpr`` input data is present in a directory which
Sarus is configured to automatically mount inside the container ( here referred
by the arbitrary variable ``$INPUT`` ), we can run the container on 16 nodes as
follows:

.. code-block:: bash

   srun -C gpu -N16 srun sarus run --mpi \
       ethcscs/gromacs:2018.3-cuda9.2_mpich3.1.4 \
       /usr/local/gromacs/bin/mdrun_mpi -s ${INPUT}/benchmark.tpr -ntomp 12

A typical output will look like:

.. code-block:: bash

                      :-) GROMACS - mdrun_mpi, 2018.3 (-:
	...
	Using 4 MPI processes
	Using 12 OpenMP threads per MPI process
	
	On host nid00001 1 GPU auto-selected for this run.
	Mapping of GPU IDs to the 1 GPU task in the 1 rank on this node: PP:0
	NOTE: DLB will not turn on during the first phase of PME tuning
	starting mdrun 'Her1-Her1'
	10000 steps,     20.0 ps.
	
	               Core t (s)   Wall t (s)        (%)
	       Time:    20878.970      434.979     4800.0
	                 (ns/day)    (hour/ns)
	Performance:        3.973        6.041
	
	GROMACS reminds you: "Shake Yourself" (YES)

If the system administrator did not configure Sarus to mount the input data
location during container setup, we can use the ``--mount`` option:

.. code-block:: bash

   srun -C gpu -N16 sarus run --mpi \
       --mount=type=bind,src=<path-to-input-directory>,dst=/gromacs-data \
       ethcscs/gromacs:2018.3-cuda9.2_mpich3.1.4 \
       /usr/local/gromacs/bin/mdrun_mpi -s /gromacs-data/benchmark.tpr -ntomp 12

Running the native application
==============================
As the native application we use GROMACS release 2018.3, compiled with FFTW
3.3.6, CUDA 9.1 and AVX2-256 SIMD instructions. The application is built by CSCS
staff and provided to the users of Piz Daint through an environment module.

Container image and Dockerfile
==============================
The container image ``ethcscs/gromacs:2018.3-cuda9.2_mpich3.1.4`` (based on
cuda/9.2 and mpich/3.1) used for this test case can be pulled from CSCS
`DockerHub
<https://hub.docker.com/r/ethcscs/gromacs/tags>`_ or be rebuilt with this
:download:`Dockerfile </cookbook/dockerfiles/gromacs/Dockerfile_2018.3>`:

.. literalinclude:: /cookbook/dockerfiles/gromacs/Dockerfile_2018.3
   :language: docker
   :linenos:

Required OCI hooks
==================
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)

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
implementation, with a small but consistent performance advantage and
comparable standard deviations across the different node counts.
