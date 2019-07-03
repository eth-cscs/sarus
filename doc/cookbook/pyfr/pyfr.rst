****
PyFR
****

`PyFR <http://www.pyfr.org>`_ is a Python code for solving high-order
computational fluid dynamics problems on unstructured grids. It leverages
symbolic manipulation and runtime code generation in order to run different
high-performance backends on a variety of hardware platforms. The
characteristics of Flux Reconstruction make the method suitable for efficient
execution on modern streaming architectures, and PyFR has been demonstrated to
achieve good portability [1]_ and scalability on some of the world's most
powerful HPC systems: most notably, a contribution based on PyFR was selected
as one of the finalists for the 2016 ACM Gordon Bell Prize [2]_.

Test case
=========
As test case, we select the 2D Euler vortex example bundled with the PyFR
source code. We run the example on 4 nodes with GPU acceleration enabled.  We
do not measure performance with this test and we do not make comparisons with a
native implementation.

Running the container
=====================
We assume that a host scratchpad directory is configured to be mounted
automatically inside the container by Sarus, and is accessible at the path
defined by ``$SCRATCH``. First step is to use an interactive container to
prepare the simulation data:

.. code-block:: bash

   # Launch interactive container
   srun -C gpu -N1 -t10 --pty sarus run --tty \
        ethcscs/pyfr:1.8.0-cuda9.2-ubuntu16.04 bash

.. code-block:: bash

   # From the container:
   ## copy the example data to the scratchpad directory
   mkdir $SCRATCH/pyfr/
   cd PyFR-1.8.0/examples/
   cp -r euler_vortex_2d/ $SCRATCH/pyfr/euler_vortex_2d

   ## Convert mesh data to PyFR format
   cd $SCRATCH/pyfr/euler_vortex_2d
   pyfr import euler_vortex_2d.msh euler_vortex_2d.pyfrm

   ## Partition the mesh and exit the container
   pyfr partition 4 euler_vortex_2d.pyfrm .
   exit

Now that the data is ready, we can launch the multi-node simulation. Notice that
we use the ``--pty`` option to ``srun`` in order to visualize and update correctly
PyFR's progress bar (which we request with the ``-p`` option):

.. code-block:: bash

   srun -C gpu -N4 -t1 --pty sarus run --mpi \
       ethcscs/pyfr:1.8.0-cuda9.2-ubuntu16.04 \
       pyfr run -b cuda -p \
       $SCRATCH/pyfr/euler_vortex_2d/euler_vortex_2d.pyfrm \
       $SCRATCH/pyfr/euler_vortex_2d/euler_vortex_2d.ini

A typical output will look like:

.. code-block:: bash

    100.0% [===========================>] 100.00/100.00 ela: 00:00:29 rem: 00:00:00


Container image and Dockerfile
==============================
The container image `ethcscs/pyfr:1.8.0-cuda9.2-ubuntu16.04` (based on Nvidia
cuda/9.2) used for this test case can be pulled from CSCS `DockerHub
<https://hub.docker.com/r/ethcscs/pyfr/tags>`_ or be rebuilt with this
:download:`Dockerfile </cookbook/dockerfiles/pyfr/Dockerfile>`:

.. literalinclude:: /cookbook/dockerfiles/pyfr/Dockerfile
   :language: docker
   :linenos:

Used OCI hooks
==============
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)


.. rubric:: References

.. [1] F.D. Witherden, B.C. Vermeire, P.E. Vincent,Heterogeneous computing on
       mixed unstructured grids with PyFR, Computers & Fluids, Volume 120, 2015,
       Pages 173-186, ISSN 0045-7930,
       https://doi.org/10.1016/j.compfluid.2015.07.016

.. [2] P. Vincent, F. Witherden, B. Vermeire, J. S. Park and A. Iyer, "Towards
       Green Aviation with Python at Petascale", SC '16: Proceedings of the
       International Conference for High Performance Computing, Networking, Storage
       and Analysis, Salt Lake City, UT, 2016, pp. 1-11.
       https://doi.org/10.1109/SC.2016.1
