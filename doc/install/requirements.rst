************
Requirements
************


Operating System
================

A Linux system with the following kernel modules loaded:

* ext4
* loop
* squashfs
* overlayfs


Software
========

.. _requirements-packages:

System packages
---------------

For Debian/Ubuntu systems:

.. code-block:: bash

    $ sudo apt-get install build-essential sudo curl wget rsync \
        autoconf automake libtool valgrind xfsprogs squashfs-tools \
        libcap-dev cmake zlib1g-dev libssl-dev


For RHEL/CentOS:

.. code-block:: bash

    $ sudo yum install gcc-c++ sudo curl wget rsync autoconf automake libtool \
        valgrind xfsprogs squashfs-tools libcap-devel cmake3 zlib-devel \
        openssl-devel

Python 2.7 is required if you are interested to also run the integration tests:

.. code-block:: bash

    # Debian/Ubuntu
    $ sudo apt-get install python python-pip

    # RHEL/CentOS
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

* `libarchive <https://github.com/libarchive/libarchive>`_ 3.3.1
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

.. code-block:: bash

    # install libarchive
    mkdir -p libarchive/3.3.1 && cd libarchive/3.3.1
    wget https://github.com/libarchive/libarchive/archive/v3.3.1.tar.gz
    tar xvf v3.3.1.tar.gz
    mv libarchive-3.3.1 src
    mkdir src/build-cmake && cd src/build-cmake
    cmake ..
    make -j$(nproc)
    make install

    # install Boost
    mkdir -p boost/1_65_0 && cd boost/1_65_0
    wget https://downloads.sourceforge.net/project/boost/boost/1.65.0/boost_1_65_0.tar.bz2
    tar xf boost_1_65_0.tar.bz2
    mv boost_1_65_0 src && cd src
    ./bootstrap.sh
    ./b2 install

    # install cpprestsdk
    mkdir -p cpprestsdk/v2.10.0 && cd cpprestsdk/v2.10.0
    wget https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz
    tar xf v2.10.0.tar.gz
    mv cpprestsdk-2.10.0 src && cd src/Release
    mkdir build && cd build
    cmake -DWERROR=FALSE ..
    make -j$(nproc)
    make install

    # install RapidJSON
    wget -O rapidjson.tar.gz https://github.com/Tencent/rapidjson/archive/663f076c7b44ce96526d1acfda3fa46971c8af31.tar.gz
    tar xvzf rapidjson.tar.gz && cd rapidjson-663f076c7b44ce96526d1acfda3fa46971c8af31
    cp -r include/rapidjson /usr/local/include/rapidjson

.. note::
    Should you have trouble pointing to a specific version of Boost when
    building the C++ REST SDK, use the `-DBOOST_ROOT` CMake option with the
    prefix directory to your Boost installation.


.. _requirements-oci-runtime:

OCI-compliant runtime
---------------------

Sarus internally relies on an OCI-compliant runtime to spawn a container.

Here we will provide some indications to install `runc
<https://github.com/opencontainers/runc>`_, the reference implementation from
the Open Container Initiative. The recommended version is **v1.0.0-rc9**.

The simplest solution is to download a pre-built binary release from the
project's GitHub page:

.. code-block:: bash

    $ wget -O runc.amd64 https://github.com/opencontainers/runc/releases/download/v1.0.0-rc9/runc.amd64
    $ chmod 755 runc.amd64                           # make it executable
    $ mv runc.amd64 /usr/local/bin/                  # put it in your PATH
    $ sudo chown root:root /usr/local/bin/runc.amd64 # set root ownership for security

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

.. code-block:: bash

    $ wget -O tini https://github.com/krallin/tini/releases/download/v0.18.0/tini
    $ chmod 755 tini                           # make it executable
    $ sudo mv tini /usr/local/bin/             # add it to PATH
    $ sudo chown root:root /usr/local/bin/tini # set root ownership for security

Alternatively, you can follow the instructions to `build from source
<https://github.com/krallin/tini#building-tini>`__.
