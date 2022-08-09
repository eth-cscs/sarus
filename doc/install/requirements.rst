************
Requirements
************

This page documents the requirements and dependencies needed by Sarus.
It serves both as a general reference and as preliminary step to performing
installations from source.

The Spack package manager is able to build or retrieve software dependencies on
its own: as such, when using Spack, the only requirements to satisfy from this
page are those related to the operating system.


Operating System
================

* Linux kernel >= 3.0
* `util-linux <https://github.com/karelzak/util-linux>`_ >= 2.20 (these utilities are usually bundled with the Linux distribution itself;
  v2.20 was released in August 2011)
* The following kernel modules loaded:

  * loop
  * squashfs
  * overlayfs

These requirements are tracked by the `CI/check_host.sh <https://github.com/eth-cscs/sarus/blob/master/CI/check_host.sh>`_ script.


.. _requirements-packages:

System packages
===============

The required packages for building Sarus are listed below for a selection of
popular Linux distributions. Please note that, depending on the distribution,
not all dependencies might be available through the system's package manager,
or some dependencies might be provided in versions not supported by Sarus.
Please follow the :ref:`manual installation <requirements-manual-installation>`
instructions for such dependencies.

**OpenSUSE Leap 15.3**:

.. literalinclude:: ../../CI/installation/install_packages_opensuseleap:15.3.sh
   :language: bash
   :start-after: set -ex
   :end-before: # DOCS: END

**CentOS 7**:

.. literalinclude:: ../../CI/installation/install_packages_centos:7.sh
   :language: bash
   :start-after: set -ex
   :end-before: # DOCS: END

**Fedora 35**:

.. literalinclude:: ../../CI/installation/install_packages_fedora:35.sh
   :language: bash
   :start-after: set -ex
   :end-before: # DOCS: END

**Debian 11**:

.. literalinclude:: ../../CI/installation/install_packages_debian:11.sh
   :language: bash
   :start-after: set -ex
   :end-before: # DOCS: END

**Ubuntu 22.04**:

.. literalinclude:: ../../CI/installation/install_packages_ubuntu:22.04.sh
   :language: bash
   :start-after: set -ex
   :end-before: # DOCS: END


.. _requirements-manual-installation:

Manual installation
===================

This section provides instructions to install some notable Sarus dependencies
which might not be available through the system's package manager, or might
be provided in a version not supported by Sarus.

Boost libraries
---------------

`Boost libraries <https://www.boost.org/>`_ are required to be version **1.60.x**
or later. The recommended version, which is used routinely for build
integration and testing, is **1.77.x**.

.. note::
    The following instructions will default to ``/usr/local`` as the installation
    prefix. To install to a specific location, use the ``--prefix`` option of
    the `b2` builder/installer tool.

.. literalinclude:: ../../CI/installation/install_dep_boost.bash
   :language: bash
   :start-after: set -ex

Skopeo
------

`Skopeo <https://github.com/containers/skopeo>`_ is used to acquire images and
their metadata from a variety of sources. Versions **1.7.0 or later** are
recommended for improved performance when pulling or loading images.

Up-to-date instructions about building Skopeo from source are available on the
project's `GitHub repository <https://github.com/containers/skopeo/blob/main/install.md#building-from-source>`_.

Umoci
-----

Umoci is used to unpack OCI images' filesystem contents before converting them
into the SquashFS format:

.. literalinclude:: ../../CI/installation/install_dep_umoci.bash
   :language: bash
   :start-after: set -ex

.. _requirements-oci-runtime:

OCI-compliant runtime
---------------------

Sarus internally relies on an OCI-compliant runtime to spawn a container.

Here we will provide some indications to install `runc
<https://github.com/opencontainers/runc>`_, the reference implementation from
the Open Container Initiative. The recommended version is **v1.1.3**.

.. note::
    runc versions from 1.1.0 to 1.1.2 have a bug which causes an error when
    Sarus attempts to propagate the PMI2 interface into containers.
    The bug has been fixed in runc 1.1.3.

The simplest solution is to download a pre-built binary release from the
project's GitHub page:

.. literalinclude:: ../../CI/installation/install_dep_runc.bash
   :language: bash
   :start-after: set -ex

Alternatively, you can follow the instructions to `build from source
<https://github.com/opencontainers/runc#building>`__, which allows more
fine-grained control over runc's features, including security options.


.. _requirements-init-process:

Init process
------------

Sarus can start an init process within containers in order to reap zombie
processes and allow container applications to receive signals.

Here we will provide some indications to install `tini
<https://github.com/krallin/tini>`_, a very lightweight init process which is
also used by Docker. The recommended version is **v0.19.0**.

The simplest solution is to download a pre-built binary release from the
project's GitHub page:

.. literalinclude:: ../../CI/installation/install_dep_tini.bash
   :language: bash
   :start-after: set -ex

Alternatively, you can follow the instructions to `build from source
<https://github.com/krallin/tini#building-tini>`__.


Python packages for integration tests
=====================================

The following Python 3 packages are required if you are interested to also run
the integration tests:

.. code-block:: bash

    $ pip3 install setuptools
    $ pip3 install pytest gcovr pexpect
