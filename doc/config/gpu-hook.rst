NVIDIA GPU hook
===============

NVIDIA provides access to GPU devices and their driver stacks inside OCI
containers through the NVIDIA Container Runtime hook, which acts as a driver for
the `nvidia-container-cli <https://github.com/NVIDIA/libnvidia-container>`_
utility.

Dependencies
------------

At least one NVIDIA GPU device and the NVIDIA CUDA Driver must be correctly
installed and working in the host system.

The hook depends on the library and utility provided by the libnvidia-container
project to carry out the low-level actions of importing the GPU device and
drivers inside the container. At the time of writing, the latest release of
libnvidia-container is version 1.0.0 RC2:

.. code-block:: bash

    # Retrieve the libnvidia-container tarball
    $ wget https://github.com/NVIDIA/libnvidia-container/releases/download/v1.0.0-rc.2/libnvidia-container_1.0.0-rc.2_x86_64.tar.xz

    # Extract the archive and install the files
    $ tar -xf libnvidia-container_1.0.0-rc.2_x86_64.tar.xz
    $ cd libnvidia-container_1.0.0-rc.2/usr/local/
    $ sudo mkdir /usr/local/libnvidia-container_1.0.0-rc.2
    $ sudo cp -r * /usr/local/libnvidia-container_1.0.0-rc.2

Hook installation
-----------------

The easiest way of installing the NVIDIA Container Runtime hook is to retrieve
the pre-compiled binary for release v.1.4.0-1:

.. code-block:: bash

    # Download the hook binary
    $ wget https://github.com/NVIDIA/nvidia-container-runtime/releases/download/v1.4.0-1/nvidia-container-runtime-hook.amd64

    # Copy to an installation directory
    $ sudo cp nvidia-container-runtime-hook.amd64 /opt/sarus/bin

To ensure correct functionality, the hook also needs a TOML configuration file
to be present on the system, and will look for it in the default path
``/etc/nvidia-container-runtime/config.toml``, unless instructed otherwise.
The configuration file is platform specific (for example, it tells the hook
where to find the system's ``ldconfig``). NVIDIA provides basic flavors for
Ubuntu, Debian, and CentOS:

.. code-block:: bash

    # Download and install hook config.toml (e.g. for CentOS)
    $ wget https://raw.githubusercontent.com/NVIDIA/nvidia-container-runtime/master/hook/config.toml.centos
    $ sudo mkdir /etc/nvidia-container-runtime/
    $ sudo cp config.toml.centos /etc/nvidia-container-runtime/config.toml

Hook installation from source
-----------------------------

In the event that you require to run a version for which NVIDIA does not
provide a pre-built binary, it is necessary to build the hook from source.

To do so, an installation of the `Go programming language
<https://golang.org/>`_ is needed. System packages are available for major Linux
distributions, or you can follow the official documentation for `manual
installation <https://golang.org/doc/install>`_.

For a regular installation, the default Go working directory is ``$HOME/go``.
Should you prefer a different location, remember to set it as the ``$GOPATH`` in
your environment.

You can now proceed to build the desired hook release from source. For example,
to get the bleeding edge:

.. code-block:: bash

    # Download and build the nvidia-container-runtime-hook
    $ go get -ldflags "-s -w" -v github.com/NVIDIA/nvidia-container-runtime/hook/nvidia-container-runtime-hook

    # Copy the hook binary to an installation directory
    $ sudo cp $GOPATH/bin/nvidia-container-runtime-hook /opt/sarus/bin/

Complete the setup by installing the ``config.toml`` file in your system.

Sarus configuration
---------------------

The NVIDIA Container Runtime hook is meant to run as a **prestart** hook. It
also expects to receive its own name/location as the first program argument, and
the string ``prestart`` as positional argument. Any other positional argument
will cause the hook to return immediately without performing any action.

The hook environment also needs to grant visibility to the library and utility
of libnvidia-container.

The following is an example ``OCIHooks`` object enabling the GPU hook:

.. code-block:: json

    {
        "prestart": [
            {
                "path": "/opt/sarus/bin/nvidia-container-runtime-hook.amd64",
                "args": ["/opt/sarus/bin/nvidia-container-runtime-hook.amd64", "prestart"],
                "env": [
                    "PATH=/usr/local/libnvidia-container_1.0.0-rc.2/bin",
                    "LD_LIBRARY_PATH=/usr/local/libnvidia-container_1.0.0-rc.2/lib"
                ]
            }
        ]
    }

------------

If you installed the configuration file in a custom location, you can
enter the ``-config=/path/to/config.toml`` flag before the ``prestart``
positional argument. For example:

.. code-block:: json

    {
        "prestart": [
            {
                "path": "/opt/sarus/bin/nvidia-container-runtime-hook",
                "args": ["/opt/sarus/bin/nvidia-container-runtime-hook", "-config=/opt/sarus/etc/nvidia-hook-config.toml", "prestart"],
                "env": [
                    "PATH=/usr/local/libnvidia-container_1.0.0-rc.2/bin",
                    "LD_LIBRARY_PATH=/usr/local/libnvidia-container_1.0.0-rc.2/lib"
                ]
            }
        ]
    }

Sarus support at runtime
------------------------

The actions performed by the NVIDIA Container Runtime hook are controlled via a
set of specific `environment variables
<https://github.com/NVIDIA/nvidia-container-runtime#environment-variables-oci-spec>`_.
Most of these can (and should) come from the container images, or from the
:ref:`user-environmental-transfer` performed by Sarus. Notably, the
``NVIDIA_VISIBLE_DEVICES`` variable defines which GPUs will be made accessible
inside the container by the hook.

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
