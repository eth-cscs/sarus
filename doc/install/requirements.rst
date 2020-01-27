************
Requirements
************


Operating System
================

A Linux system with the following kernel modules loaded:

* loop
* squashfs
* overlayfs


Software
========

.. _requirements-packages:

System packages
---------------

For Debian-based systems (tested on Debian 10 and Ubuntu 18.04):

.. literalinclude:: ../../CI/installation/install_packages_ubuntu:18.04.sh
   :language: bash
   :start-after: # Install packages

For CentOS 7:

.. literalinclude:: ../../CI/installation/install_packages_centos:7.sh
   :language: bash
   :start-after: # Install packages

Python 2.7 is required if you are interested to also run the integration tests:

.. code-block:: bash

    # Debian/Ubuntu
    $ sudo apt-get install python python-pip

    # CentOS
    $ sudo yum install python python2-pip

    # All platforms, after installing Python + pip
    $ pip install setuptools
    $ pip install nose gcovr pexpect

.. note::
    If you plan to install Sarus using the Spack package manager, you can skip
    the rest of this page, since the remaining dependencies will be installed by
    Spack itself.


Additional dependencies
-----------------------

* `libarchive <https://github.com/libarchive/libarchive>`_ 3.4.1
* `Boost libraries <https://www.boost.org/>`_ >= 1.60.x (recommended 1.65.x)
* `C++ REST SDK <https://github.com/Microsoft/cpprestsdk>`_ v2.10.0
* `RapidJSON <http://rapidjson.org/index.html>`_ commit 663f076

.. important::
    We recommend these versions as they are the ones routinely used for build
    integration and testing, thus guaranteed to work.

As the specific software versions listed above may not be provided by the system
package manager, we suggest to install from source:

.. note::
    The following instructions will default to ``/usr/local`` as the installation
    prefix. To install to a specific location, use the ``-DCMAKE_INSTALL_PREFIX``
    CMake options for libarchive and C++ REST SDK and the ``--prefix`` option for
    the Boost libraries.

.. literalinclude:: ../../CI/installation/install_dependencies.sh
   :language: bash
   :start-after: # Dependencies from source --------------
   :end-before: # Pre-built dependencies --------------

.. note::
    Should you have trouble pointing to a specific version of Boost when
    building the C++ REST SDK, use the ``-DBOOST_ROOT`` CMake option with the
    prefix directory to your Boost installation.


.. _requirements-oci-runtime:

OCI-compliant runtime
---------------------

Sarus internally relies on an OCI-compliant runtime to spawn a container.

Here we will provide some indications to install `runc
<https://github.com/opencontainers/runc>`_, the reference implementation from
the Open Container Initiative. The recommended version is **v1.0.0-rc10**.

The simplest solution is to download a pre-built binary release from the
project's GitHub page:

.. literalinclude:: ../../CI/installation/install_dependencies.sh
   :language: bash
   :start-after: # Install runc
   :end-before: # Install tini

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
also used by Docker. The recommended version is **v0.18.0**.

The simplest solution is to download a pre-built binary release from the
project's GitHub page:

.. literalinclude:: ../../CI/installation/install_dependencies.sh
   :language: bash
   :start-after: # Install tini

Alternatively, you can follow the instructions to `build from source
<https://github.com/krallin/tini#building-tini>`__.
