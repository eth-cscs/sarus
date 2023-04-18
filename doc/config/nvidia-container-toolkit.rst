*****************************
NVIDIA Container Runtime hook
*****************************

NVIDIA provides access to GPU devices and their driver stacks inside OCI
containers through an OCI hook called ``NVIDIA Container Runtime hook``
(part of the `NVIDIA Container Toolkit <https://github.com/NVIDIA/nvidia-container-toolkit>`_),
which acts as a driver for the
`nvidia-container-cli <https://github.com/NVIDIA/libnvidia-container>`_
utility.

Dependencies
============

At least one NVIDIA GPU device and the NVIDIA CUDA Driver must be correctly
installed and working in the host system.

The hook depends on the library and utility provided by the
`libnvidia-container <https://github.com/NVIDIA/libnvidia-container>`_
project to carry out the low-level actions of importing the GPU device and
drivers inside the container. At the time of writing, the latest release of
``libnvidia-container`` is version 1.13.0. The most straightforward way to obtain
the library and its CLI utility binary is to configure the related
`package repository <https://nvidia.github.io/libnvidia-container/>`_
for your Linux distribution and install the pre-built packages. For example, on
a RHEL-based distribution:

.. code-block:: bash

    # Configure the NVIDIA repository
    $ DIST=$(. /etc/os-release; echo $ID$VERSION_ID)
    $ curl -s -L https://nvidia.github.io/libnvidia-container/$DIST/libnvidia-container.repo | \
          sudo tee /etc/yum.repos.d/libnvidia-container.repo

    # Install the packages
    $ sudo yum install libnvidia-container1 libnvidia-container-tools

Installation
============

At the time of writing, the latest revision of the NVIDIA Container Runtime hook
is version 1.13.0.

System packages
---------------

NVIDIA provides distribution-specific packages for their Container Toolkit (which includes the hook)
through the `libnvidia-container repository <https://nvidia.github.io/libnvidia-container/>`_.

For example, to install the hook on RHEL 8:

.. code-block::

    # Configure the NVIDIA repository
    $distribution=$(. /etc/os-release;echo $ID$VERSION_ID) \
    $ curl -s -L https://nvidia.github.io/libnvidia-container/$distribution/libnvidia-container.repo | \
          sudo tee /etc/yum.repos.d/nvidia-container-toolkit.repo

    # Install the package
    sudo dnf clean expire-cache \
    && sudo dnf install -y nvidia-container-toolkit

More information is available on the `official NVIDIA documentation
<https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html>`_.

Build from source
-----------------

To build the hook from source, an installation of the `Go programming language
<https://golang.org/>`_ is needed. System packages are available for major Linux
distributions, or you can follow the official documentation for `manual
installation <https://golang.org/doc/install>`_.

For a regular installation, the default Go working directory is ``$HOME/go``.
Should you prefer a different location, remember to set it as the ``$GOPATH`` in
your environment.

You can now proceed to build the hook:

.. code-block:: bash
    
    $ git clone https://github.com/NVIDIA/nvidia-container-toolkit.git
    $ cd nvidia-container-toolkit
    $ git checkout v1.13.0
    $ make cmd-nvidia-container-runtime-hook

    # Copy the hook binary to an installation directory
    $ sudo cp ./nvidia-container-runtime-hook /opt/sarus/bin/nvidia-container-runtime-hook-1.13.0

To ensure correct functionality, the hook also needs a TOML configuration file
to be present on the system, and will look for it in the default path
``/etc/nvidia-container-runtime/config.toml``, unless instructed otherwise.
The configuration file is platform specific (for example, it tells the hook
where to find the system's ``ldconfig``). NVIDIA provides basic flavors for
Ubuntu, Debian, OpenSUSE Leap, and distributions based on the YUM/DNF package manager
(e.g. CentOS, Fedora, Amazon Linux 2):

.. code-block:: bash

    # Install hook config.toml (e.g. for OpenSUSE Leap)
    $ sudo mkdir /etc/nvidia-container-runtime/
    $ sudo cp <NVIDIA Container Toolkit git repo>/config/config.toml.opensuse-leap /etc/nvidia-container-runtime/config.toml

Configuration
=============

The NVIDIA Container Runtime hook is meant to run as a **prestart** hook. It
also expects to receive its own name/location as the first program argument, and
the string ``prestart`` as positional argument. Any other positional argument
will cause the hook to return immediately without performing any action.

The hook environment also needs to grant visibility to the libnvidia-container
library (e.g. ``libnvidia-container.so``) and CLI utility (``nvidia-container-cli``).

Below is an example of `OCI hook JSON configuration file
<https://github.com/containers/common/blob/main/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the NVIDIA Container Runtime hook. Notice that in this example the
``-config=/path/to/config.toml`` flag is entered before the ``prestart``
positional argument to point the hook to a configuration file installed in a
custom location. Such flag is not required if you installed the TOML configuration
file in the custom location as described in the previous section.

.. literalinclude:: /config/hook_examples/03-nvidia-container-toolkit.json
   :language: json

.. _config-hooks-nvidia-support:

Sarus support at runtime
========================

The actions performed by the NVIDIA Container Runtime hook are controlled via a
set of specific `environment variables
<https://github.com/NVIDIA/nvidia-container-runtime#environment-variables-oci-spec>`_.
Most of these can (and should) come from container images or from the
host environment (see :ref:`here <user-environment>` for more about environments
in Sarus containers).
Notably, the ``NVIDIA_VISIBLE_DEVICES`` variable defines which GPUs will be made
accessible inside the container by the hook.

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
