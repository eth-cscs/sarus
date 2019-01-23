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
powerful HPC systems: most notably, a contribution based on PyFR was selected as
one of the finalists for the 2016 ACM Gordon Bell Prize [2]_.

Test case
=========
As test case, we select the 2D Euler vortex example bundled
with the PyFR source code. We run the example on 4 nodes with
GPU acceleration enabled.
We do not measure performance with this test and we do not make comparisons
with a native implementation.

Container image and Dockerfile
==============================
We build a Docker image for PyFR 1.8.0, which we base on the official Docker
image for the CUDA Toolkit 9.2 and add: Python 3.5.2, the MPICH 3.1.4 MPI
implementation, the `Metis library for mesh partitioning
<http://glaros.dtc.umn.edu/gkhome/metis/metis/overview>`_, the specific
dependencies for PyFR, and finally PyFR itself.
Bundling the Metis library is very convenient, as it makes
the container able to carry out mesh partitioning on its own without relying on
tools from the host.

.. code-block:: docker

   FROM nvidia/cuda:9.2-devel-ubuntu16.04

   LABEL com.pyfr.version="1.8.0"
   LABEL com.python.version="3.5"

   # Install system dependencies
   RUN apt-get update && apt-get install -y   \
           unzip                       \
           wget                        \
           build-essential             \
           gfortran-5                  \
           strace                      \
           realpath                    \
           libopenblas-dev             \
           liblapack-dev               \
           python3-dev                 \
           python3-setuptools          \
           python3-pip                 \
           libhdf5-dev                 \
           libmetis-dev                \
           --no-install-recommends  && \
       rm -rf /var/lib/apt/lists/*

   # Install MPICH 3.1.4
   RUN wget -q http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz   && \
       tar xvf mpich-3.1.4.tar.gz                       && \
       cd mpich-3.1.4                                   && \
       ./configure --disable-fortran --prefix=/usr      && \
       make -j$(nproc)                                  && \
       make install                                     && \
       cd ..                                            && \
       rm -rf mpich-3.1.4.tar.gz mpich-3.1.4            && \
       ldconfig

   # Create new user
   RUN useradd docker
   WORKDIR /home/docker

   # Install Python dependencies
   RUN pip3 install numpy>=1.8         \
                    pytools>=2016.2.1  \
                    mako>=1.0.0        \
                    appdirs>=1.4.0     \
                    mpi4py>=2.0     && \
       pip3 install pycuda>=2015.1     \
                    h5py>=2.6.0     && \
       wget -q -O GiMMiK-2.1.tar.gz https://github.com/vincentlab/GiMMiK/archive/v2.1.tar.gz && \
       tar -xvzf GiMMiK-2.1.tar.gz  && \
       cd GiMMiK-2.1                && \
       python3 setup.py install     && \
       cd ..                        && \
       rm -rf GiMMiK-2.1.tar.gz GiMMiK-2.1

   # Set base directory for pyCUDA cache
   ENV XDG_CACHE_HOME /tmp

   # Install PyFR
   RUN wget -q -O PyFR-1.8.0.zip http://www.pyfr.org/download/PyFR-1.8.0.zip && \
       unzip -qq PyFR-1.8.0.zip     && \
       cd PyFR-1.8.0                && \
       python3 setup.py install     && \
       cd ..                        && \
       rm -rf PyFR-1.8.0.zip

   CMD ["pyfr --help"]

Running the container
=====================
We assume that a host scratchpad directory is configured to be mounted
automatically inside the container by Sarus, and is accessible at the path
defined by ``$SCRATCH``.

First, we use an interactive container to prepare the simulation data:

.. code-block:: bash

   # Launch interactive container
   srun -C gpu -N1 -t10 --pty sarus run --tty ethcscs/pyfr:1.8.0-cuda9.2-ubuntu16.04 bash

   # Copy the example data to the scratchpad directory
   mkdir $SCRATCH/pyfr/
   cd PyFR-1.8.0/examples/
   cp -r euler_vortex_2d/ $SCRATCH/pyfr/euler_vortex_2d

   # Convert mesh data to PyFR format
   cd $SCRATCH/pyfr/euler_vortex_2d
   pyfr import euler_vortex_2d.msh euler_vortex_2d.pyfrm

   # Partition the mesh and exit the container
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

    100.0% [===========================>] 100.00/100.00 ela: 00:00:29 rem: 00:00:00


.. rubric:: References

.. [1] F.D. Witherden, B.C. Vermeire, P.E. Vincent,Heterogeneous computing on mixed unstructured grids with PyFR,
       Computers & Fluids, Volume 120, 2015, Pages 173-186, ISSN 0045-7930, https://doi.org/10.1016/j.compfluid.2015.07.016
.. [2] P. Vincent, F. Witherden, B. Vermeire, J. S. Park and A. Iyer, "Towards Green Aviation with Python at Petascale",
       SC '16: Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis,
       Salt Lake City, UT, 2016, pp. 1-11. https://doi.org/10.1109/SC.2016.1
