**********
User guide
**********

Basic end-to-end user workflow
==============================

Steps to do **on your workstation**:

1. Build the container image using the Docker tool and Dockerfiles.

2. Push the Docker image into Docker Hub registry (https://hub.docker.com).

Steps to do **on the HPC system**:

3. Pull the Docker image from the Docker Hub registry using Sarus.

4. Run the image at scale with Sarus.

An explanation of the different steps required to deploy a Docker image using
Sarus follows.

1. Develop the Docker image
---------------------------

First, the user has to build a container image. This boils down to
writing a Dockerfile that describes the container, executing the Docker
command line tool to build the image, and then running the container to
test it. Below you can find an overview of what the development process
looks like. We provide a brief introduction to using Docker, however,
for a detailed explanation please refer to the `Docker Get Started
guide <https://docs.docker.com/engine/getstarted/>`_.

Let's start with a simple example. Consider the following Dockerfile
where we install Python on a Debian Jessie base image::

    FROM debian:jessie
    RUN apt-get -y update && apt-get install -y python

Once that the user has written the Dockerfile, the container
image can be built:

.. code-block:: bash

    $ docker build -t hello-python .

The previous step will take the content of the Dockerfile and build our
container image. The first entry, ``FROM debian:jessie``, will specify a base
Linux distribution image as a starting point for our container (in this case,
Debian Jessie), the image of which Docker will try to fetch from its registry if
it is not already locally available. The second entry ``RUN apt-get -y update &&
apt-get install -y python`` will execute the command that follows the ``RUN``
instruction, updating the container with the changes done to its software
environment: in this case, we are installing Python using the apt-get package
manager.

Once the build step has finished, we can list the available container
images using:

.. code-block:: bash

    $ docker images
    REPOSITORY        TAG        IMAGE ID         CREATED           SIZE
    hello-python      latest     2e57316387c6     3 minutes ago     224 MB

We can now spawn a container from the image we just built (tagged as
``hello-python``), and run Python inside the container
(``python --version``), so as to verify the version of the installed
interpreter:

.. code-block:: bash

    $ docker run --rm hello-python python --version
    Python 2.7.9

One of the conveniences of building containers with Docker is that this
process can be carried out solely on the user's workstation/laptop,
enabling quick iterations of building, modifying, and testing of
containerized applications that can then be easily deployed on a variety
of systems, greatly improving user productivity.

.. note::

    Reducing the size of the container image, besides saving disk space,
    also speeds up the process of importing it into Sarus later on.
    The easiest ways to limit image size are cleaning the package manager cache
    after installations and deleting source codes and other intermediate build
    artifacts when building software manually.
    For practical examples and general good advice on writing Dockerfiles,
    please refer to the official `Best practices for writing Dockerfiles
    <https://docs.docker.com/engine/userguide/eng-image/dockerfile_best-practices/>`_.

A user can also access the container interactively through a shell,
enabling quick testing of commands. This can be a useful step for
evaluating commands before adding them to the Dockerfile:

.. code-block:: bash

    $ docker run --rm -it hello-python bash
    root@c5fc1954b19d:/# python --version
    Python 2.7.9
    root@c5fc1954b19d:/#

2. Push the Docker image to the Docker Hub
------------------------------------------

Once the image has been built and tested, you can log in to `DockerHub <https://hub.docker.com/>`_
(requires an account) and push it, so that it becomes available from the cloud:

.. code-block:: bash

    $ docker login
    $ docker push <user name>/<repo name>:<image tag>

Note that in order for the push to succeed, the image has to be
correctly tagged with the same ``<repository name>/<image name>:<image tag>``
identifier you intend to push to. Images can be tagged at build-time, supplying
the ``-t`` option to :program:`docker build`, or afterwards by using
:program:`docker tag`. In the case of our example:

.. code-block:: bash

    $ docker tag hello-python <repo name>/hello-python:1.0


The image tag (the last part of the identifier after the colon) is optional.
If absent, Docker will set the tag to ``latest`` by default.

3. Pull the Docker image from Docker Hub
----------------------------------------

Now the image is stored in the Docker Hub and you can pull it into the HPC
system using the :program:`sarus pull` command followed by the image
identifier:

.. code-block:: bash

    $ sarus pull <repo name>/hello-python:1.0

While performing the pull does not require specific privileges, it is generally
advisable to run :program:`sarus pull` on the system's compute nodes through
the workload manager: compute nodes often have better hardware and, in some
cases like Cray XC systems, large RAM filesystems, which will greatly reduce
the pull process time and will allow to pull larger images.

Should you run into problems because the pulled image doesn't fit in the default
filesystem, you can specify an alternative temporary directory with the
``--temp-dir`` option.

You can use :program:`sarus images` to list the images available on the system:

.. code-block:: bash

    $ sarus images
    REPOSITORY                 TAG       DIGEST         CREATED               SIZE         SERVER
    <repo name>/hello-python   1.0       6bc9d2cd1831   2018-01-19T09:43:04   40.16MB      index.docker.io

4. Run the image at scale with Sarus
--------------------------------------

Once the image is available to Sarus we can run it at scale using the workload
manager. For example, if using SLURM:

.. code-block:: bash

    $ srun -N 1 sarus run <repo name>/hello-python:1.0 python --version
    Python 2.7.9

As with Docker, containers can also be used through a terminal,
enabling quick testing of commands:

.. code-block:: bash

    $ srun -N 1 --pty sarus run -t debian bash
    $ cat /etc/os-release
    PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
    NAME="Debian GNU/Linux"
    VERSION_ID="9"
    VERSION="9 (stretch)"
    ID=debian
    HOME_URL="https://www.debian.org/"
    SUPPORT_URL="https://www.debian.org/support"
    BUG_REPORT_URL="https://bugs.debian.org/"

    $ exit

The ``--pty`` option to ``srun`` and the ``-t/--tty`` option to ``sarus run``
are needed to properly setup the pseudo-terminals in order to achieve a familiar
user experience.

You can tell the previous example was run inside a container by querying the
specifications of your host system. For example, the OS of Cray XC compute nodes
is based on SLES and not Debian:

.. code-block:: bash

    $ srun -N 1 cat /etc/os-release
    NAME="SLES"
    VERSION="12"
    VERSION_ID="12"
    PRETTY_NAME="SUSE Linux Enterprise Server 12"
    ID="sles"
    ANSI_COLOR="0;32"
    CPE_NAME="cpe:/o:suse:sles:12"


Additional features
===================

Pulling images from private or 3rd party registries
---------------------------------------------------

By default, Sarus tries to pull images from `DockerHub <https://hub.docker.com/>`_
public repositories. To perform a pull from a private repository, use the
``--login`` option to the :program:`sarus pull` command and enter your
credentials:

.. code-block:: bash

    $ srun -N 1 sarus pull --login <privateRepo>/<image>:<tag>
    username    :user
    password    :
    ...

To pull an image from a registry different from Docker Hub, enter the server
address as part of the image identifier. For example, to access the NVIDIA GPU
Cloud:

.. code-block:: bash

    $ srun -N 1 sarus pull --login nvcr.io/nvidia/k8s/cuda-sample:nbody
    username    :$oauthtoken
    password    :
    ...

To work with images not pulled from Docker Hub (including the removal detailed
in a later section), you need to enter the image descriptor (repository[:tag])
as displayed by the :program:`sarus images` command in the first two columns:

.. code-block:: bash

    $ sarus images
    REPOSITORY                       TAG          DIGEST         CREATED               SIZE         SERVER
    nvcr.io/nvidia/k8s/cuda-sample   nbody        29e2298d9f71   2019-01-14T12:22:25   91.88MB      nvcr.io

    $ srun -N1 sarus run nvcr.io/nvidia/k8s/cuda-sample:nbody cat /usr/local/cuda/version.txt
    CUDA Version 9.0.176


Loading images from tar archives
--------------------------------

If you do not have access to a remote Docker registry, or you're uncomfortable
with uploading your images to the cloud in order to pull them, Sarus offers
the possibility to load images from tar archives generated by
`docker save <https://docs.docker.com/engine/reference/commandline/save/>`_.

First, save an image to a tar archive using Docker *on your workstation*:

.. code-block:: bash

    $ docker save --output debian.tar debian:jessie
    $ ls -sh
    total 124M
    124M debian.tar

Then, transfer the archive to the HPC system and use the :program:`sarus load`
command, followed by the archive filename and the descriptor you want to give to
the Sarus image:

.. code-block:: bash

    $ sarus images
    REPOSITORY       TAG          DIGEST         CREATED      SIZE         SERVER

    $ srun sarus load ./debian.tar my_debian
    > expand image layers ...
    > extracting     : /tmp/debian.tar/7e5c6402903b327fc62d1144f247c91c8e85c6f7b64903b8be289828285d502e/layer.tar
    > make squashfs ...
    > create metadata ...
    # created: <user home>/.sarus/images/load/library/my_debian/latest.squashfs
    # created: <user home>/.sarus/images/load/library/my_debian/latest.meta

    $ sarus images
    REPOSITORY               TAG          DIGEST         CREATED               SIZE         SERVER
    load/library/my_debian   latest       2fe79f06fa6d   2018-01-31T15:08:56   47.04MB      load

The image is now ready to use. Notice that the origin server for the image has
been labeled ``load`` to indicate this image has been loaded from an archive.

Similarly to :program:`sarus pull`, we recommend to load tar archives from
compute nodes. Should you run out of space while expanding the image,
:program:`sarus load` also accepts the ``--temp-dir`` option to specify an
alternative expansion directory.

As with images from 3rd party registries, to use or remove loaded images you
need to enter the image descriptor (repository[:tag]) as displayed by the
:program:`sarus images` command in the first two columns.

Removing images
---------------

To remove an image from Sarus's local repository, use the :program:`sarus
rmi` command:

.. code-block:: bash

    $ sarus images
    REPOSITORY       TAG          DIGEST         CREATED               SIZE         SERVER
    library/debian   latest       6bc9d2cd1831   2018-01-31T14:11:27   40.17MB      index.docker.io

    $ sarus rmi debian:latest
    removed index.docker.io/library/debian/latest

    $ sarus images
    REPOSITORY   TAG          DIGEST         CREATED      SIZE         SERVER

To remove images pulled from 3rd party registries or images loaded from local
tar archives you need to enter the image descriptor (repository[:tag]) as
displayed by the :program:`sarus images` command:

.. code-block:: bash

    $ sarus images
    REPOSITORY               TAG          DIGEST         CREATED               SIZE         SERVER
    load/library/my_debian   latest       2fe79f06fa6d   2018-01-31T15:08:56   47.04MB      load

    $ sarus rmi load/library/my_debian
    removed load/library/my_debian/latest


Accessing host directories from the container
---------------------------------------------

System administrators can configure Sarus to automatically mount facilities
like parallel filesystems into every container. Refer to your site documentation
or system administrator to know which resources have been enabled on a specific
system.

.. _user-environmental-transfer:

Environmental Transfer
----------------------

All the environment variables defined in the calling processes' environment
will be transferred into the container, however, any environment variables
defined in the container description, e.g., Docker ENV-defined variables,
will be sourced and override those.

.. _user-custom-mounts:

Mounting custom directories into the container
----------------------------------------------

By default, Sarus creates the container filesystem environment from the image
and host system as specified by the system administrator.
It is possible to request additional paths from the host environment to be
mapped to some other path within the container using the ``--mount`` option of
the :program:`sarus run` command:

.. code-block:: bash

    $ srun -N 1 --pty sarus run --mount=type=bind,source=/path/to/my/data,destination=/data -t debian bash

The previous command would cause ``/data`` within the container to be bound to
``/scratch/path/to/my/data``.
The option can be specified multiple times, one for each mount to be performed.
``--mount`` accepts a comma-separated list of ``<key>=<value>`` pairs as its
argument, much alike the Docker option with the same name (for reference, see
the `official Docker documentation on bind mounts
<https://docs.docker.com/engine/admin/volumes/bind-mounts/>`_). As with Docker,
the order of the keys is not significant. A detailed breakdown of the possible
flags follows in the next subsections.

Mandatory flags
^^^^^^^^^^^^^^^

* ``type``: represents the type of the mount. Currently, only ``bind``
  (for bind-mounts) is supported
* ``destination``: Absolute path to where the filesystem will be made available
  inside the container. If the directory does not exist, it will be created.
  It is possible to overwrite other bind mounts already present in the
  container, however, the system administrator retains the power to disallow
  user-requested mounts to any location at his/her discretion.
  May alternatively be specified as ``dst`` or ``target``.

Bind mounts
^^^^^^^^^^^
Regular bind mounts can specify the following flags:

* ``source`` (required): Absolute path accessible from the user *on the host*
  that will be mounted in the container. Can alternatively be specified as ``src``.
* ``readonly`` (optional): Causes the filesystem to be mounted as read-only.
  This flag takes no value.
* ``bind-propagation`` (optional): Specifies the type of bind propagation to
  use for the mount. Can be one of ``recursive``, ``slave``, ``private``,
  ``rslave``, ``rprivate`` (the last two values stand for "recursive
  private" and "recursive slave" respectively).

The following example demonstrates the use of a custom read-only bind mount.

.. code-block:: bash

    $ ls -l /input_data
    drwxr-xr-x.  2 root root      57 Feb  7 10:49 ./
    drwxr-xr-x. 23 root root    4096 Feb  7 10:49 ../
    -rw-r--r--.  1 root root 1048576 Feb  7 10:49 data1.csv
    -rw-r--r--.  1 root root 1048576 Feb  7 10:49 data2.csv
    -rw-r--r--.  1 root root 1048576 Feb  7 10:49 data3.csv

    $ echo "1,2,3,4,5" > data4.csv

    $ srun -N 1 --pty sarus run --mount=type=bind,source=/input_data,destination=/input,readonly -t debian bash
    $ ls -l /input
    -rw-r--r--. 1 root 0 1048576 Feb  7 10:49 data1.csv
    -rw-r--r--. 1 root 0 1048576 Feb  7 10:49 data2.csv
    -rw-r--r--. 1 root 0 1048576 Feb  7 10:49 data3.csv
    -rw-r--r--. 1 root 0      10 Feb  7 10:52 data4.csv

    $ cat /input/data4.csv
    1,2,3,4,5

    $ touch /input/data5.csv
    touch: cannot touch '/input/data5.csv': Read-only file system

    $ exit


.. _user-entrypoint-default-args:

Image entrypoint and default arguments
--------------------------------------

Sarus fully supports image entrypoints and default arguments as defined by the
OCI Image Specification.

The entrypoint of an image is a list of arguments that will be used as the
command to execute when the container starts; it is meant to create an image
that will behave like an executable file.

The image default arguments will be passed to the entrypoint if no argument is
provided on the commmand line when launching a container. If the entrypoint is
not present, the first default argument will be treated as the executable to
run.

When creating container images with Docker, the entrypoint and default arguments
are set using the `ENTRYPOINT
<https://docs.docker.com/engine/reference/builder/#entrypoint>`_ and `CMD
<https://docs.docker.com/engine/reference/builder/#cmd>`_ instructions
respectively in the Dockerfile. For example, this file will generate an image
printing arguments to the terminal by default::

    FROM debian:stretch

    ENTRYPOINT ["/bin/echo"]
    CMD ["Hello world"]

After building such image (we'll arbitrarily call it ``echo``) and importing it
into Sarus, we can run it without passing any argument:

.. code-block:: bash

    $ srun sarus run <image repo>/echo
    Hello world

Entering a command line argument will override the default arguments passed to
the entrypoint:

.. code-block:: bash

    $ srun sarus run <image repo>/echo foobar
    Foobar

The image entrypoint can be changed by providing a value to the ``--entrypoint``
option of :program:`sarus run`. It is important to note that when changing the
entrypoint the default arguments get discarded as well:

.. code-block:: bash

    $ srun sarus run --entrypoint=cat <image repo>/echo /etc/os-release
    PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
    NAME="Debian GNU/Linux"
    VERSION_ID="9"
    VERSION="9 (stretch)"
    ID=debian
    HOME_URL="https://www.debian.org/"
    SUPPORT_URL="https://www.debian.org/support"
    BUG_REPORT_URL="https://bugs.debian.org/"

The entrypoint can be removed by using ``--entrypoint`` without arguments. This
is useful, for example, for inspecting and debugging containers:

.. code-block:: bash

    $ srun --pty sarus run --entrypoint -t <image repo>/echo bash
    $ env | grep ^PATH
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

    $ exit

Verbosity levels and help messages
----------------------------------

To run a command in verbose mode, enter the ``--verbose`` global option before
the command:

.. code-block:: bash

    $ srun sarus --verbose run debian:latest cat /etc/os-release

To run a command printing extensive details about internal workings, enter the ``--debug`` global
option before the command:

.. code-block:: bash

    $ srun sarus --debug run debian:latest cat /etc/os-release

To print a general help message about Sarus, use ``sarus --help``.

To print information about a command (e.g. command-specific options), use
``sarus help <command>``:


.. code-block:: bash

    $ sarus help run
    Usage: sarus run [OPTIONS] REPOSITORY[:TAG] [COMMAND] [ARG...]

    Run a command in a new container

    Note: REPOSITORY[:TAG] has to be specified as
          displayed by the "sarus images" command.

    Options:
      --centralized-repository  Use centralized repository instead of the local one
      -t [ --tty ]              Allocate a pseudo-TTY in the container
      --entrypoint arg          Overwrite the default ENTRYPOINT of the image
      --mount arg               Mount custom directories into the container
      -m [ --mpi ]              Enable MPI support
      --ssh                     Enable SSH in the container


Support for container customization through hooks
=================================================

Sarus allows containers to be customized by other programs or scripts
leveraging the interface defined by the Open Container Initiative Runtime
Specification for POSIX-platform hooks (OCI hooks for short). These
customizations are especially amenable to HPC use cases, where the dedicated
hardware and highly-tuned software adopted by high-performance systems are in
contrast with the platform-agnostic nature of software containers. OCI hooks
provide a solution to open access to these resources inside containers.

The hooks that will be enabled on a given Sarus installation are configured
by the system administrators. Please refer to your site documentation or your
system administrator to know which hooks are available in a specific system and
how to activate them.

Here we will illustrate a few cases of general interest for HPC from an end-user
perspective.

Native MPI support (MPICH-based)
--------------------------------

Sarus comes with a hook able to import native MPICH-based MPI implementations
inside the container. This is useful in case the host system features a
vendor-specific or high-performance MPI stack based on MPICH (e.g. Intel MPI,
Cray MPT, MVAPICH) which is required to fully leverage a high-speed
interconnect.

To take advantage of this feature, the MPI installed in the container (and
dynamically linked to your application) needs to be ABI-compatible with
the MPI on the host system. Taking as an example the Piz Daint
Cray XC50 supercomputer at CSCS, to best meet the required
`ABI-compatibility <https://www.mpich.org/abi/>`_ we recommend that the
container application uses one of the following MPI implementations:

- `MPICH v3.1.4 <http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz>`_ (Feburary 2015)
- `MVAPICH2 2.2 <http://mvapich.cse.ohio-state.edu/download/mvapich/mv2/mvapich2-2.2.tar.gz>`_ (September 2016)
- Intel MPI Library Library 2017 Update 1

The following is an example Dockerfile to create a Debian image with MPICH
3.1.4::

    FROM debian:jessie

    RUN apt-get update && apt-get install -y \
            build-essential             \
            wget                        \
            --no-install-recommends     \
        && rm -rf /var/lib/apt/lists/*


    RUN wget -q http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz \
        && tar xf mpich-3.1.4.tar.gz \
        && cd mpich-3.1.4 \
        && ./configure --disable-fortran --enable-fast=all,O3 --prefix=/usr \
        && make -j$(nproc) \
        && make install \
        && ldconfig \
        && cd .. \
        && rm -rf mpich-3.1.4 \
        && rm mpich-3.1.4.tar.gz

.. note::

    Applications that are statically linked to MPI libraries
    will not work with the native MPI support provided by the hook.

Once the system administrator has configured the hook, containers with native
MPI support, can be launched by passing the ``--mpi`` option to the
:program:`sarus run` command, e.g.:

.. code-block:: bash

    $ srun -N 16 -n 16 sarus run --mpi <repo name>/<image name> <mpi_application>

NVIDIA GPU support
------------------

NVIDIA provides access to GPU devices and their driver stacks inside OCI
containers through the NVIDIA container runtime hook.

When Sarus is configured to use this hook, the GPU devices to be made
available inside the container can be selected by setting the
``CUDA_VISIBLE_DEVICES`` environment variable in the host system. Such action is
often performed automatically by the workload manager or other site-specific
software (e.g. the SLURM workload manager sets ``CUDA_VISIBLE_DEVICES`` when
GPUs are requested via the `Generic Resource Scheduling plugin
<https://slurm.schedmd.com/gres.html>`_). Be sure to check the setup provided by
your computing site.

The container image needs to include a CUDA runtime that is suitable for both
the target container applications and the available GPUs in the host system. One
way to achieve this is by basing your image on one of the official Docker images
provided by NVIDIA, i.e. the Dockerfile should start with a line like this::

    FROM nvidia/cuda:8.0

.. note::

    To check all the available CUDA images provided by NVIDIA, visit
    https://hub.docker.com/r/nvidia/cuda/

When developing GPU-accelerated images on your workstation, we recommend using
`nvidia-docker <https://github.com/NVIDIA/nvidia-docker>`_ to run and test
containers using an NVIDIA GPU.

SSH connection within containers
--------------------------------

Sarus also comes with a hook which enables support for SSH connections within
containers.

When Sarus is configured to use this hook, first of all, it is necessary to
run the command ``sarus ssh-keygen``. This command generates the SSH keys that
will be used by the SSH daemons as well as the SSH clients in the containers.

It is then possible to execute a container passing the ``--ssh`` option to
:program:`sarus run`, e.g. ``sarus run --ssh <image> <command>``. Using the
previously generated the SSH keys, the hook will instantiate an sshd and setup a
custom ``ssh`` binary inside each container created with the same command.

Within a container spawned with the ``--ssh`` option, it is possible to connect
into other containers by simply issuing the ``ssh`` command available in the
default search ``PATH``. E.g.:

.. code-block:: bash

    ssh <hostname of other node>

The custom ``ssh`` binary will take care of using the proper keys and
non-standard port in order to connect to the remote container.

OpenMPI communication through SSH
---------------------------------

The MPICH-based MPI hook described above does not support OpenMPI libraries.
As an alternative, OpenMPI programs can communicate through SSH connections
created by the SSH hook.

To run an OpenMPI program using the SSH hook, we need to manually provide a list
of hosts and explicitly launch ``mpirun`` only on one node of the allocation.
We can do so with the following commands:

.. code-block:: bash

   salloc -C gpu -N4 -t5
   srun hostname >$HOME/hostfile
   srun sarus run --ssh --mount=src=/users,dst=/users,type=bind \
       ethcscs/openmpi:3.1.3  \
       bash -c 'if [ $SLURM_PROCID -eq 0 ]; then mpirun --hostfile $HOME/hostfile -npernode 1 /openmpi-3.1.3/examples/hello_c; else sleep infinity; fi'
