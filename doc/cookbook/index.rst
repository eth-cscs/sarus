Sarus Cookbook
==============

This cookbook is a collection of documented use case examples of the Sarus
container runtime for HPC. Each example is accompanied by material and
information sufficient to reproduce it. In this sense, the examples also serve
as recipes demonstrating how to build containers with features like MPI and
CUDA, and how to leverage those features from Sarus. The examples are ordered by
increasing complexity, providing a smooth learning curve.


.. toctree::
   :maxdepth: 1
   :caption: Contents:

   test_setup.rst
   gpu/cuda_nbody.rst
   gpu/gpudirect.rst
   osu_mb/osu_mb.rst
   openmpi/openmpi_ssh.rst
   gromacs/gromacs.rst
   tensorflow_horovod/tf_hvd.rst
   pyfr/pyfr.rst
