************
Installation
************

Sarus can be installed in two alternative ways:

    - :ref:`Using the Spack package manager <installation-spack>`
    - :ref:`Manual installation <installation-manual>`

Whichever you choose, please also read the section about :ref:`Minimal configuration
<installation-minimal-config>` and the :ref:`Additional remarks before startup
<installation-before-startup>` to ensure everything is set to run Sarus.

.. _installation-spack:

Installing with Spack
=====================

Sarus provides `Spack <https://spack.io/>`_ packages for itself and its
dependencies which are not yet covered by Spack's builtin repository.
Installing with Spack is convenient because detection and installation of
dependencies are handled automatically.

As explained in the :ref:`execution requirements
<requirements-permissions-execution>`, Sarus must be installed as a root-owned
SUID binary. The straightforward way to achieve this is running the installation
command with super-user privileges.
Another option is to install as a regular user and ``chown`` to root after that.

The installation procedure with Spack is as follows:

.. code-block:: bash

   # Setup Spack bash integration (if you haven't already done so)
   . ${SPACK_ROOT}/share/spack/setup-env.sh

   # Create a local Spack repository for Sarus-specific dependencies
   export SPACK_LOCAL_REPO=${SPACK_ROOT}/var/spack/repos/cscs
   spack repo create ${SPACK_LOCAL_REPO}
   spack repo add ${SPACK_LOCAL_REPO}

   # Import Spack packages for Cpprestsdk, RapidJSON and Sarus
   cp -r <Sarus project root dir>/spack/packages/* ${SPACK_LOCAL_REPO}/packages/

   # Install Sarus
   spack install --verbose sarus

By default, the latest tagged release will be installed. To get the bleeding edge,
use the ``@develop`` version specifier.

The Spack package for Sarus supports the following `variants <https://spack.readthedocs.io/en/latest/basic_usage.html#basic-variants>`_
to customize the installation:

   - ``ssh``: Build and install the SSH hook and custom OpenSSH software to enable
     connections inside containers [True].

For example, in order to perform a quick installation without SSH we could use:

.. code-block:: bash

   spack install --verbose sarus ssh=False


.. _installation-manual:

Manual Installation
===================

Build
-----

Change directory to the Sarus's sources:

.. code-block:: bash

   cd <Sarus project root dir>

Create a new folder to build Sarus out-of-source:

.. code-block:: bash

   mkdir build
   cd build

Configure and build:

.. code-block:: bash

   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake \
         -DCMAKE_INSTALL_PREFIX=/opt/sarus \
         ..
   make

.. note::
    CMake should automatically find the dependencies (include directories,
    shared objects, and binaries). However, should CMake not find a dependency,
    its location can be manually specified through the command line. E.g.::

       cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake \
             -DCMAKE_INSTALL_PREFIX=/opt/sarus \
             -DCMAKE_PREFIX_PATH="<boost install dir>;<cpprestsdk install dir>;<libarchive install dir>;<rapidjson install dir>" \
             -Dcpprestsdk_INCLUDE_DIR=<cpprestsdk include dir> \
             ..

.. note::
    Old versions of CMake might have problems finding Boost 1.65.0. We recommend to use at least CMake 3.10 in order to avoid compatibility issues.

Below are listed the Sarus-specific options that can be passed to CMake in
order to customize your build:

   - CMAKE_INSTALL_PREFIX: installation directory of Sarus [/usr/local].
   - ENABLE_SSH: build and install the SSH hook and custom OpenSSH software to enable
     connections inside containers [TRUE].
   - ENABLE_TESTS_WITH_VALGRIND: run each unit test through valgrind [FALSE].

Install
-------

As explained in the :ref:`execution requirements
<requirements-permissions-execution>`, Sarus must be installed as a root-owned
SUID binary. The straightforward way to achieve this is running the ``make
install`` command with super-user privileges:

.. code-block:: bash

    sudo make install

Another option is to install as a regular user and ``chown`` to root after that.

To complete the installation, create the directory in which Sarus will create the OCI
bundle for containers. The location of this directory is configurable at any time, as
described in the next section. As an example, taking default values:

.. code-block:: bash

    sudo mkdir <sarus installation dir>/var/OCIBundleDir


.. _installation-minimal-config:

Minimal configuration
=====================

At run time, Sarus takes its configuration options from a file named
*sarus.json*. This file must be placed in the directory ``CMAKE_INSTALL_PREFIX/etc``.

A *sarus.json* file with a minimal configuration is automatically created in
``CMAKE_INSTALL_PREFIX/etc`` as part of the installation step.

Here we will highlight some key settings which form a baseline configuration.
For the full details about configuration options and the structure of *sarus.json*
please consult the :doc:`/config/configuration_reference`.

* **securityChecks:** enable runtime security checks (root ownership of files, etc.).
  Disabling this may be convenient when rapidly iterating over test and development
  installations. It is strongly recommended to keep these checks enabled for
  production deployments
* **OCIBundleDir:** the absolute path to where Sarus will create the OCI
  bundle for the container. This directory must satisfy the :ref:`security
  requirements <requirements-permissions-security>` for critical files and
  directories.
  By default, the OCI bundle directory is located in
  ``<installation path>/var/OCIBundleDir``.
* **localRepositoryBaseDir:** the starting path to individual user directories,
  where Sarus will create (if necessary) and access local repositories.
  The repositories will be located in ``<localRepositoryBaseDir>/<user name>/.sarus``.
* **runcPath:** the absolute path to an OCI-compliant runtime which will be used
  by Sarus to spawn containers. When configuring the build, CMake will search
  for runc in the system path. If you installed runc in a custom location, or
  are using a different runtime, you will have to edit this path manually.
* **siteMounts:** a list of JSON objects defining filesystem mounts that will be
  automatically performed from the host system into the container.
  This is typically used to make network filesystems accessible within the
  container but could be used to allow certain other facilities.
  Each object in the list has to define ``type``, ``source``, ``destination``
  and optionally ``flags`` for the mount. Please refer to the
  :ref:`configuration reference <config-reference-siteMounts>` for the complete
  format and features of these entries.
* **ramFilesystemType:** the type of temporary filesystem Sarus will use to
  setup the base filesystem for the container. The OCI  bundle, and consequently
  the container's rootfs, will be generated in a filesystem of this type. The
  default value of ``tmpfs`` is indicated for most platforms.

    .. important::
        **Known issue on CLE**

        When using Sarus on the Cray Linux Environment, the value of the
        configuration option **ramFilesystemType** should be set to **ramfs**.
        Using the default recommended value, i.e. **tmpfs**, will not work on Cray
        Compute Nodes.


.. _installation-before-startup:

Additional remarks before startup
=================================

Required kernel modules
-----------------------

If the kernel modules listed in :doc:`requirements` are not loaded automatically
by the system, remember to load them manually:

.. code-block:: bash

    sudo modprobe ext4
    sudo modprobe loop
    sudo modprobe squashfs
    sudo modprobe overlay


Sarus's passwd cache
--------------------

During the installation, the passwd and group information are copied and cached
into *<sarus install prefix>/etc/passwd* and *<sarus install prefix>/etc/group*
respectively. The cache allows to bypass the host's passwd/group database, e.g.
LDAP, which could be tricky to configure and access from the container. However,
since the cache is created/updated only once at installation time, it can
quickly get out-of-sync with the actual passwd/group information of the system.
A possible solution is to periodically run a cron job to refresh the
cache. E.g. a cron job and a script like the ones below would do:

.. code-block:: bash

    $ crontab -l
    5 0 * * * update_sarus_user.sh

.. code-block:: bash

    $ cat update_sarus_user.sh

    #!/bin/bash

    /usr/bin/getent passwd > <sarus install prefix>/etc/passwd
    /usr/bin/getent group  > <sarus install prefix>/etc/group
