************
Installation
************

Installation
============

Build
-----

Change directory to the Sarus's sources:

.. code-block:: bash

   cd <sarus source dir>

Create a new folder to build Sarus out-of-source:

.. code-block:: bash

   mkdir build
   cd build

Configure and build:

.. code-block:: bash

   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake
   make

.. note::
    CMake should automatically find the dependencies (include directories,
    shared objects, and binaries). However, should CMake not find a dependency,
    its location can be manually specified through the command line. E.g.::

       cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake \
             -DCMAKE_PREFIX_PATH="<boost install dir>;<cpprestsdk install dir>;<libarchive install dir>" \
             -Dcpprestsdk_INCLUDE_DIR=<cpprestsdk include dir> \
             ..

Below are listed the Sarus-specific options that can be passed to CMake in
order to customize your build:

   - CMAKE_INSTALL_PREFIX: installation directory of Sarus [/opt/sarus].
   - SYSCONFDIR: directory with the configuration files of Sarus [<CMAKE_INSTALL_PREFIX>/etc].
   - DIR_OF_FILES_TO_COPY_IN_CONTAINER_ETC: directory with configuration files that are copied into the container [<CMAKE_INSTALL_PREFIX>/files_to_copy_in_container_etc].
   - ENABLE_RUNTIME_SECURITY_CHECKS: Enable runtime security checks (root ownership of files, etc.).
     Disabling this may be convenient when rapidly iterating over test and development installations.
     It is strongly recommended to keep these checks enabled for production deployments [TRUE].
   - ENABLE_SSH: build and install the SSH hook and custom OpenSSH software to enable
     connections inside containers [TRUE].
   - ENABLE_TESTS_WITH_GCOV: run gcov after each unit test to collect coverage information [FALSE].
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


Minimal configuration
=====================

At run time, Sarus takes its configuration options from a file named
*sarus.json*. This file must be placed in the directory specified to CMake
with ``SYSCONFDIR``, e.g. ``cmake -DSYSCONFDIR=/opt/sarus/default/etc``. If
not specified, ``SYSCONFDIR`` defaults to ``CMAKE_INSTALL_PREFIX/etc``. For reference, a
template with a default configuration named *sarus.json.example* is created in
``SYSCONFDIR`` as part of the installation step.

Here we will highlight some key settings to complete a baseline configuration.
For the full details about configuration options and the structure of *sarus.json*
please consult the :doc:`/config/configuration_reference`.

* **OCIBundleDir:** the absolute path to where Sarus will create the OCI
  bundle for the container. This directory must satisfy the :ref:`security
  requirements <requirements-permissions-security>` for critical files and
  directories.
  E.g., when using the default value of ``/var/sarus/OCIBundleDir``:

  .. code-block:: bash

      $ sudo mkdir -p /var/sarus/OCIBundleDir

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
    sudo modprobe overlayfs


Sarus's passwd cache
----------------------

During the installation, the passwd information is copied and cached into
*<sarus install dir>/files_to_copy_in_container_etc/passwd*. The cache is supposed to allow the
Sarus runtime to perform quicker accesses to the passwd information. However,
since the cache is created/updated only once at installation time, it can
quickly get out-of-sync with the actual passwd information of the system. A
possible solution/workaround is to periodically run a cron job to refresh the
cache. E.g. a cron job and a script like the ones below would do:

.. code-block:: bash

    $ crontab -l
    5 0 * * * update_sarus_user.sh

.. code-block:: bash

    $ cat update_sarus_user.sh

    #!/bin/bash

    /usr/bin/getent passwd > <sarus install dir>/files_to_copy_in_container_etc/passwd
