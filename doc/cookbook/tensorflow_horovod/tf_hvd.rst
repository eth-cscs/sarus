***********************
TensorFlow with Horovod
***********************

`TensorFlow <https://www.tensorflow.org/>`_ is a software framework providing an
API to express numerical computations using data flow graphs. It also provides
an implementation to run those computations on a broad array of platforms, from
mobile devices to large systems with heterogeneous environments. While usable in
a variety of scientific domains, it is mainly intended for the development of
machine-learning (ML) models, with a focus on deep neural networks. The
development of TensorFlow was started internally at Google Inc., and the
software was released as open source in November 2015.

`Horovod <https://github.com/horovod/horovod>`_ is a framework developed by Uber
Technologies Inc. to perform distributed training of deep neural networks on top
of another ML framework, like TensorFlow, Keras, or PyTorch. Notably, it allows
to replace TensorFlow's own parameter server architecture for distributed
training with communications based on an MPI model, leveraging ring-allreduce
algorithms for improved usability and performance.

Horovod 0.15.1 with CUDA 9.x
============================
As test case, we select the `tf_cnn_benchmark
<https://github.com/tensorflow/benchmarks>`_ scripts from the Tensorflow project
for benchmarking convolutional neural networks. We use a `ResNet-50 model
<https://arxiv.org/abs/1512.03385>`_ with a batch size of 64 and the synthetic
image data which the benchmark scripts are able to generate autonomously. We
performed runs from a minimum of 2 nodes up to 128 nodes, increasing the node
count in powers of two.

Running the container
---------------------
Assuming that the tensorflow-benchmark code is present in a directory which Sarus is
configured to automatically mount inside the container (here referred by the
arbitrary variable ``$INPUT``), we can run the container application as follows:

.. code-block:: bash

   srun -C gpu -N4 -t5 sarus run --mpi \
       ethcscs/horovod:0.15.1-tf1.7.0-cuda9.0-mpich3.1.4-no-nccl \
       python ${INPUT}/tensorflow-benchmarks/scripts/tf_cnn_benchmarks/tf_cnn_benchmarks.py \
       --model resnet50 --batch_size 64 --variable_update horovod

If the system administrator did not configure Sarus to mount the input data
location during container setup, we can use the ``--mount`` option:

.. code-block:: bash

   srun -C gpu -N4 -t5 sarus run --mpi \
       --mount=type=bind,src=<path-to-parent-directory>/tensorflow-benchmarks/scripts/,dst=/tf-scripts \
       ethcscs/horovod:0.15.1-tf1.7.0-cuda9.0-mpich3.1.4-no-nccl \
       python /tf-scripts/tf_cnn_benchmarks/tf_cnn_benchmarks.py \
       --model resnet50 --batch_size 64 --variable_update horovod

Native application
------------------
For the native implementation, we use TensorFlow 1.7.0, built using Cray Python
3.6, the Python extensions provided by the Cray Programming Environment 18.08,
CUDA 9.1 and cuDNN 7.1.4. These installations are performed by CSCS staff and
are available on Piz Daint through environment modules. We complete the software
stack by creating a virtual environment with Cray Python 3.6 and installing
Horovod 0.15.1 through the ``pip`` utility. The virtual environment is
automatically populated with system-installed packages from the loaded
environment modules.

Container image and Dockerfile
------------------------------
We start from the reference Dockerfile provided by Horovod for version 0.15.1
and modify it to use Python 3.5, TensorFlow 1.7.0, CUDA 9.0, cuDNN 7.0.5. These
specific versions of CUDA and cuDNN are required because they are the ones
against which the version of TensorFlow available through ``pip`` has been
built. We also replace OpenMPI with MPICH 3.1.4 and remove the installation of
OpenSSH, as the containers will be able to communicate between them thanks to
Slurm and the native MPI hook. Finally, we instruct Horovod not to use NVIDIA's
NCCL library for any MPI operation, because NCCL is not available natively on
Piz Daint.
The final :download:`Dockerfile </cookbook/dockerfiles/tensorflow_horovod/Dockerfile_horovod_0_15_1>`
is the following:

.. literalinclude:: /cookbook/dockerfiles/tensorflow_horovod/Dockerfile_horovod_0_15_1
    :language: docker

Used OCI hooks
--------------
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)

Results
-------
We measure the performance in images/sec as reported by the application logs and
compute speedup values using the performance averages for each data point,
taking the native performance at 2 nodes as baseline. The results are showcased
in the following Figure:

.. _fig-horovod-results_old:

.. figure:: horovod-results_old.*
   :scale: 100%
   :alt: TensorFlow with Horovod results

   Comparison of performance and speedup between native and Sarus-deployed
   container versions of TensorFlow with Horovod on Piz Daint.

We observe the container application closely matching the native installation
when running on up to 16 nodes, with performance differences and normalized
standard deviations less than 0.5%. From 32 nodes upwards, the container
application shows a small performance advantage, up to 5% at 128 nodes,
with both implementations maintaining close standard deviation values.

Horovod 0.16.x with CUDA 10.0
=============================
In this test case, we select again the `tf_cnn_benchmark
<https://github.com/tensorflow/benchmarks>`_ scripts from the Tensorflow project
but now we test all four different models that the benchmark supports, namely
the *alexnet*, *inception3*, *resnet50* and *vgg16*. The batch size is again 64
and for each of the models we use a node range of 1 to 12 nodes.

Running the container
---------------------
If the  tensorflow-benchmark code is present in a directory which Sarus is
configured to automatically mount inside the container (here referred by the
arbitrary variable ``$INPUT``), we can run the container application as follows:

.. code-block:: bash

   srun -C gpu -N4 sarus run --mpi \
       ethcscs/horovod:0.16.1-tf1.13.1-cuda10.0-mpich3.1.4-nccl \
       python ${INPUT}/tensorflow-benchmarks/scripts/tf_cnn_benchmarks/tf_cnn_benchmarks.py \
       --model resnet50 --batch_size 64 --variable_update horovod

Alternatively, the ``--mount`` option can be used:

.. code-block:: bash

   srun -C gpu -N4 -t5 sarus run --mpi \
       --mount=type=bind,src=<path-to-parent-directory>/tensorflow-benchmarks/scripts/,dst=/tf-scripts \
       ethcscs/horovod:0.16.1-tf1.13.1-cuda10.0-mpich3.1.4-nccl \
       python /tf-scripts/tf_cnn_benchmarks/tf_cnn_benchmarks.py \
       --model resnet50 --batch_size 64 --variable_update horovod

The above commands are using the ``resnet50`` model. Using the ``--model``
option it is possible to run the benchmarks with the other models as well.

Native application
------------------
For the native implementation, we use Horovod 0.16.0 with TensorFlow 1.12.0,
built using Cray Python 3.6, the Python extensions provided by the Cray
Programming Environment 19.03, CUDA 10.0, cuDNN 7.5.6 and NCCL 2.4.2. These
installations are performed by CSCS staff and are available on Piz Daint through
environment modules.

Container image and Dockerfile
------------------------------
We start from the reference Dockerfile provided by Horovod for version 0.16.1
and modify it to use Python 3.5, TensorFlow 1.13.1, CUDA 10.0, cuDNN 7.5.0. and
NCCL 2.4.2. These specific versions of CUDA and cuDNN are required because they
are the ones against which the version of TensorFlow available through ``pip``
has been built. We also replace OpenMPI with MPICH 3.1.4. and remove the
installation of OpenSSH, as the containers will be able to communicate thanks to
Slurm and the native MPI hook Finally, we instruct Horovod to use NVIDIA's NCCL
library for every MPI operation by adding the appropriate environment variables
to the **/etc/nccl.conf** configuration file.
The resulting :download:`Dockerfile </cookbook/dockerfiles/tensorflow_horovod/Dockerfile_horovod_0_16_1>`
is the following:

.. literalinclude:: /cookbook/dockerfiles/tensorflow_horovod/Dockerfile_horovod_0_16_1
    :language: docker

Used OCI hooks
--------------
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)

Results
-------
We measure the performance in images/sec as reported by the application logs by
taking the mean value based on 5 different runs for each model and node number.
The results are showcased in the following Figure:

.. _fig-horovod-results_new:

.. figure:: horovod-results_new.*
   :scale: 100%
   :alt: TensorFlow with Horovod results

   Comparison of performance between native and Sarus-deployed
   container versions of TensorFlow with Horovod on Piz Daint.


We observe that performance of the container-based horovod-tensorflow is
identical to the native one. An slight increased performance of the
containized solution is observed only for the alexnet model as the number of
nodes increases.
