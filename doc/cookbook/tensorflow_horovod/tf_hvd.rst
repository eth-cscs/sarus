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

`Horovod <https://github.com/uber/horovod>`_ is a framework developed by Uber
Technologies Inc. to perform distributed training of deep neural networks on top
of another ML framework, like TensorFlow, Keras, or PyTorch. Notably, it allows
to replace TensorFlow's own parameter server architecture for distributed
training with communications based on an MPI model, leveraging ring-allreduce
algorithms for improved usability and performance.

Test case
=========
As test case, we select the `tf_cnn_benchmark
<https://github.com/tensorflow/benchmarks>`_ scripts from the Tensorflow project
for benchmarking convolutional neural networks. We use the four different models 
that the benchmark can accept, namely the *alexnet*, *inception3*, *resnet50* 
and *vgg16* with a batch size of 64 and the synthetic
image data which the benchmark scripts are able to generate autonomously. We
performed runs from a minimum of 1 nodes up to 12 nodes.

Native application
==================
For the native implementation, we use Horovod 0.16.0 with TensorFlow 1.12.0, 
built using Cray Python 3.6, the Python extensions provided by the Cray 
Programming Environment 19.03, CUDA 10.0, cuDNN 7.5.6 and NCCL 2.4.2. These 
installations are performed by CSCS staff and are available on Piz Daint through
environment modules.

Container image and Dockerfile
==============================
We start from the reference Dockerfile provided by Horovod for version 0.16.1
and modify it to use Python 3.5, TensorFlow 1.13.1, CUDA 10.0, cuDNN 7.5.0. and
NCCL 2.4.2. These specific versions of CUDA and cuDNN are required because they 
are the ones against which the version of TensorFlow available through ``pip`` 
has been built. We also replace OpenMPI with MPICH 3.1.4. Finally, we instruct 
Horovod to use NVIDIA's NCCL library for every MPI operation.

.. code-block:: docker

    FROM nvidia/cuda:10.0-devel-ubuntu16.04

    # Define the software versions
    ENV HOROVOD_VERSION=0.16.1 \
        TENSORFLOW_VERSION=1.13.1 \
        CUDNN_VERSION=7.5.0.56-1+cuda10.0 \
        NCCL_VERSION=2.4.2-1+cuda10.0

    # Python version
    ARG python=3.5
    ENV PYTHON_VERSION=${python}

    # Install the necessary packages
    RUN apt-get update && \
        apt-get install -y --no-install-recommends \
        cmake git curl vim wget ca-certificates \
        libibverbs-dev \
        libcudnn7=${CUDNN_VERSION} \
        libnccl2=${NCCL_VERSION} \
        libnccl-dev=${NCCL_VERSION} \
        libjpeg-dev \
        libpng-dev \
        python${PYTHON_VERSION} python${PYTHON_VERSION}-dev

    # Create symbolic link in order to make the installed python default
    RUN ln -s /usr/bin/python${PYTHON_VERSION} /usr/bin/python

    # Download pip bootstrap script and install pip
    RUN curl -O https://bootstrap.pypa.io/get-pip.py && \
        python get-pip.py && \
    r   m get-pip.py

    # Install Tensorflow, Keras and h5py
    RUN pip install tensorflow-gpu==${TENSORFLOW_VERSION} keras h5py

    # Install MPICH 3.1.4
    RUN cd /tmp \
        && wget -q http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz \
        && tar xf mpich-3.1.4.tar.gz \
        && cd mpich-3.1.4 \
        && ./configure --disable-fortran --enable-fast=all,O3 --prefix=/usr \
        && make -j$(nproc) \
        && make install \
        && ldconfig \
        && cd .. \
        && rm -rf mpich-3.1.4 mpich-3.1.4.tar.gz \
        && cd /

    # Install Horovod
    RUN ldconfig /usr/local/cuda-10.0/targets/x86_64-linux/lib/stubs && \
        HOROVOD_GPU_ALLREDUCE=NCCL HOROVOD_WITH_TENSORFLOW=1 pip install --no-cache-dir horovod==${HOROVOD_VERSION} && \
    l   dconfig

    # Install OpenSSH for MPI to communicate between containers
    RUN apt-get install -y --no-install-recommends openssh-client openssh-server && \
        mkdir -p /var/run/sshd

    # Allow OpenSSH to talk to containers without asking for confirmation
    RUN cat /etc/ssh/ssh_config | grep -v StrictHostKeyChecking > /etc/ssh/ssh_config.new && \
        echo "    StrictHostKeyChecking no" >> /etc/ssh/ssh_config.new && \
        mv /etc/ssh/ssh_config.new /etc/ssh/ssh_config

    # NCCL configuration
    RUN echo NCCL_DEBUG=INFO >> /etc/nccl.conf && \
        echo NCCL_IB_HCA=ipogif0 >> /etc/nccl.conf && \
        echo NCCL_IB_CUDA_SUPPORT=1

Used OCI hooks
==============
* NVIDIA Container Runtime hook
* Native MPI hook (MPICH-based)

Running the container
=====================
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

Results
=======
We measure the performance in images/sec as reported by the application logs and
compute speedup values using the performance averages for each data point. 
The results are showcased in the following Figure:

.. _fig-horovod-results:

.. figure:: horovod-results.*
   :scale: 100%
   :alt: TensorFlow with Horovod results

   Comparison of performance and speedup between native and Sarus-deployed
   container versions of TensorFlow with Horovod on Piz Daint.


We observe the container application closely matching the native installation
when running on up to 16 nodes, with performance differences and normalized
standard deviations less than 0.5%. From 32 nodes upwards, the container
application shows a small performance advantage, up to 5% at 128 nodes,
with both implementations maintaining close standard deviation values.
