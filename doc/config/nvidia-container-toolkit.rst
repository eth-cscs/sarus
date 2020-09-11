NVIDIA Container Toolkit
========================

NVIDIA provides access to GPU devices and their driver stacks inside OCI
containers through an OCI hook called
`NVIDIA Container Toolkit <https://github.com/NVIDIA/container-toolkit>`_,
which acts as a driver for the
`nvidia-container-cli <https://github.com/NVIDIA/libnvidia-container>`_
utility.

Dependencies
------------

At least one NVIDIA GPU device and the NVIDIA CUDA Driver must be correctly
installed and working in the host system.

The Toolkit depends on the library and utility provided by the
`libnvidia-container <https://github.com/NVIDIA/libnvidia-container>`_
project to carry out the low-level actions of importing the GPU device and
drivers inside the container. At the time of writing, the latest release of
libnvidia-container is version 1.0.6. The easiest way to install libnvidia-container
is to build from source on a Docker-enabled system and copy the binaries onto
the system where Sarus is installed:

.. code-block:: bash
    
    ## ON SYSTEM WITH DOCKER ##

    # Clone the repository
    $ git clone https://github.com/NVIDIA/libnvidia-container.git
    $ cd libnvidia-container
    $ git checkout v1.0.6

    # Build with Docker
    # It is possible to build using containers based on Ubuntu (16.04/18.04),
    # Debian (9/10), CentOS 7, OpenSUSE Leap 15.1 or Amazon Linux
    $ make ubuntu18.04

    # Packages and a tarfile will be available in the dist/<distribution>/<arch>
    # directory
    $ ls dist/ubuntu18.04/amd64

    # Copy tarfile to Sarus system
    $ rsync -a dist/ubuntu18.04/amd64/libnvidia-container_1.0.6_amd64.tar.xz <user>@<target system with Sarus>


    ## ON TARGET SYSTEM ##

    # Extract the archive and install the files
    $ tar -xf libnvidia-container_1.0.6_amd64.tar.xz
    $ cd libnvidia-container_1.0.6/usr/local/
    $ sudo mkdir /usr/local/libnvidia-container_1.0.6
    $ sudo cp -r * /usr/local/libnvidia-container_1.0.6

Installation
-----------------

At the time of writing, the latest revision of the NVIDIA Container Toolkit
is commit 60f165ad69. NVIDIA no longer provides pre-built binaries for this
software, so it is necessary to build from source.

To do so, an installation of the `Go programming language
<https://golang.org/>`_ is needed. System packages are available for major Linux
distributions, or you can follow the official documentation for `manual
installation <https://golang.org/doc/install>`_.

For a regular installation, the default Go working directory is ``$HOME/go``.
Should you prefer a different location, remember to set it as the ``$GOPATH`` in
your environment.

You can now proceed to build the Toolkit from source:

.. code-block:: bash
    
    $ go get github.com/NVIDIA/container-toolkit
    $ cd $GOPATH/src/github.com/NVIDIA/container-toolkit/
    $ git checkout 60f165ad69
    $ cd $GOPATH
    $ go build -ldflags "-s -w" -v github.com/NVIDIA/container-toolkit/nvidia-container-toolkit

    # Copy the toolkit binary to an installation directory
    $ sudo cp $GOPATH/nvidia-container-toolkit /opt/sarus/bin/nvidia-container-toolkit

To ensure correct functionality, the Toolkit also needs a TOML configuration file
to be present on the system, and will look for it in the default path
``/etc/nvidia-container-runtime/config.toml``, unless instructed otherwise.
The configuration file is platform specific (for example, it tells the Toolkit
where to find the system's ``ldconfig``). NVIDIA provides basic flavors for
Ubuntu, Debian, CentOS, OpenSUSE Leap and Amazon Linux:

.. code-block:: bash

    # Install hook config.toml (e.g. for CentOS)
    $ sudo mkdir /etc/nvidia-container-runtime/
    $ sudo cp $GOPATH/src/github.com/NVIDIA/container-toolkit/config/config.toml.centos /etc/nvidia-container-runtime/config.toml

Sarus configuration
---------------------

The NVIDIA Container Toolkit is meant to run as a **prestart** hook. It
also expects to receive its own name/location as the first program argument, and
the string ``prestart`` as positional argument. Any other positional argument
will cause the Toolkit to return immediately without performing any action.

The hook environment also needs to grant visibility to the library and utility
of libnvidia-container.

Below is an example of `OCI hook JSON configuration file
<https://github.com/containers/libpod/blob/master/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the NVIDIA Container Toolkit. Notice that in this example the 
``-config=/path/to/config.toml`` flag is entered before the ``prestart``
positional argument to point the Toolkit to a configuration file installed in a
custom location. Such flag is not required if you installed the TOML configuration
file in the custom location as described in the previous section.

.. literalinclude:: /config/hook_examples/03-nvidia-container-toolkit.json
   :language: json

Sarus support at runtime
------------------------

The actions performed by the NVIDIA Container Toolkit hook are controlled via a
set of specific `environment variables
<https://github.com/NVIDIA/nvidia-container-runtime#environment-variables-oci-spec>`_.
Most of these can (and should) come from the container images, or from the
:ref:`user-environmental-transfer` performed by Sarus. Notably, the
``NVIDIA_VISIBLE_DEVICES`` variable defines which GPUs will be made accessible
inside the container by the Toolkit.

However, in an HPC scenario, the hardware resources should be assigned from a
supervisory entity, such as a workload manager. For example, the SLURM workload
manager Generic Resource Scheduling (GRES) plugin selects which GPU devices are
assigned to a job by setting the ``CUDA_VISIBLE_DEVICES`` environment variable
inside the job process.

For this reason, when preparing a container Sarus will look for
``CUDA_VISIBLE_DEVICES`` in the *host* environment, and modify accordingly both
``NVIDIA_VISIBLE_DEVICES`` and ``CUDA_VISIBLE_DEVICES`` in the *container*.
These modifications ensure that the host resource allocations are respected,
while guaranteeing the correct operation of CUDA applications inside the
container, even in the case of partial or shuffled devices selection on
multi-GPU systems.
