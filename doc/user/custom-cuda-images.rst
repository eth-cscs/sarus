***************************************
Creating custom Docker images with CUDA
***************************************

The base images provided by NVIDIA in `Docker Hub <https://hub.docker.com/r/nvidia/cuda>`_
only offer flavors based on Ubuntu and CentOS. If you want to build a CUDA-enabled
image on a different distribution, we offer the following advice:

Installing the CUDA Toolkit
===========================

* **Package manager installer:**
  repository installers (to be used through the system package manager)
  are available for Fedora, OpenSUSE, RHEL and SLES (and also for Ubuntu and CentOS,
  if you don't want to use NVIDIA's images). For detailed installation instructions,
  refer to the official CUDA Toolkit Documentation (https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#package-manager-installation).

  Please note that installing the default package options (e.g. ``cuda`` or ``cuda-tookit``)
  will add a significant amount of resources to your image (more than 1GB).
  Significant size savings can be achieved by selectively installing only the
  packages with the parts of the Toolkit that you need. Please consult your system
  package manager documentation in order to list the available packages inside
  the CUDA repositories.

* **Runfile install:**
  some distributions are not covered by the CUDA package manager and have to perform
  the installation through the standalone runfile installer.
  One such case is Debian, which is also used as base for several official Docker
  Hub images (e.g. `Python <https://hub.docker.com/_/python/>`_).  For detailed installation instructions,
  refer to the official CUDA Toolkit Documentation (https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#runfile).
  We advise to supply the ``--silent`` and ``--toolkit`` options to the installer,
  to avoid installing the CUDA drivers as well.

  The standalone installer will add a significant amount of resources to your image,
  including documentation and SDK samples. If you are really determined to reduce
  the size of your image, you can selectively ``rm -rf`` the parts of the Toolkit
  that you don't need, but be careful about not deleting libraries and tools
  that may be used by applications in the container!

Controlling the NVIDIA Container Runtime
========================================

The NVIDIA Container Runtime (at the heart of nvidia-docker v2) is controlled
through several environment variables. These can be part of the container image
or be set by :program:`docker run` with the ``-e``, ``--env`` or ``--env-file``
options (please refer to the `official
documentation <https://docs.docker.com/engine/reference/commandline/run/#set-environment-variables--e---env---env-file>`_
for the full syntax of these flags). A brief description of the most useful
variables follows:

* ``NVIDIA_VISIBLE_DEVICES``: This variable controls which GPUs will be made accessible
  inside the container. The values can be a list of indexes (e.g. ``0,1,2``), ``all``, or
  ``none``. If the variable is empty or unset, the runtime will behave as default Docker.

* ``NVIDIA_DRIVER_CAPABILITIES``: This option controls which driver libraries and binaries
  will be mounted inside the container. It accepts as values a comma-separated list of
  driver features (e.g. ``utility,compute``) or the string ``all`` (for all driver capabilities).
  An empty or unset variable will default to minimal driver capabilties (``utility``).

* ``NVIDIA_REQUIRE_CUDA``: A logical expression to define a constraint on the CUDA driver version
  required by the container (e.g. ``"cuda>=8.0"`` will require a driver supporting the CUDA
  Toolkit version 8.0 and later). Multiple constraints can be expressed in a single environment
  variable: space-separated constraints are ORed, comma-separated constraints are ANDed.

A full list of the environment variables looked up by the NVIDIA Container Runtime
and their possible values is available `here <https://github.com/NVIDIA/nvidia-container-runtime#environment-variables-oci-spec>`_.

As an example, in order to work correctly with the NVIDIA runtime, an image including
a CUDA 8.0 application could feature these instructions in its Dockerfile::

    ENV NVIDIA_VISIBLE_DEVICES all
    ENV NVIDIA_DRIVER_CAPABILITIES compute,utility
    ENV NVIDIA_REQUIRE_CUDA "cuda>=8.0"

Alternatively, the variables can be specified (or even overwritten) from the
command line:

.. code-block:: bash

    $ docker run --runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all -e NVIDIA_DRIVER_CAPABILITIES=compute,utility --rm nvidia/cuda nvidia-smi

The official ``nvidia/cuda`` images already include a set of these environment
variables. This means that all the Dockerfiles that do a ``FROM nvidia/cuda``
will automatically inherit them and thus will work seamlessly with the NVIDIA
Container Runtime.
