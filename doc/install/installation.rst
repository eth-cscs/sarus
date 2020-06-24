************
Installation
************

Sarus can be installed in three alternative ways:

    - :doc:`Using the standalone Sarus archive </quickstart/quickstart>`
    - :ref:`Using the Spack package manager <installation-spack>`
    - :ref:`Installing from source <installation-source>`

When installing from source or with Spack, make sure that the :doc:`required
dependencies </install/requirements>` are available on the system.

Whichever procedure you choose, please also read the page about
:doc:`post-installation` to ensure everything is set to run Sarus.

.. _installation-spack:

Installing with Spack
=====================

Sarus provides `Spack <https://spack.io/>`_ packages for itself and its
dependencies which are not yet covered by Spack's builtin repository.
Installing with Spack is convenient because detection and installation of
dependencies are handled automatically.

As explained in the :ref:`execution requirements
<post-installation-permissions-execution>`, Sarus must be installed as a
root-owned SUID binary in a directory hierarchy which belongs to the root user
at all levels.
The straightforward way to achieve this is to use a root-owned Spack and run the
installation procedure with super-user privileges.
An alternative procedure for test/development installations with a non-root
Spack is discussed in :ref:`this subsection<installation-spack-nonroot>`.

The installation procedure with Spack follows below.
If you are not performing these actions as the root user, prefix each ``spack``
command with ``sudo``:

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

   - ``ssh``: Build and install the SSH hook and custom SSH software to enable
     connections inside containers [True].
   - ``configure_installation``: Run the script to setup a starting Sarus
     configuration as part of the installation phase. Running the script
     requires super-user privileges [True].

For example, in order to perform a quick installation without SSH we could use:

.. code-block:: bash

   spack install --verbose sarus ssh=False

.. _installation-spack-nonroot:

Using Spack as a non-root user (for test and development only)
--------------------------------------------------------------

By default, the Spack package for Sarus will run a :ref:`configuration script
<configure-installation-script>` as part of the installation phase to setup a
minimal working configuration. The script needs to run with root privileges,
which in turn means the ``spack install`` command must be issued with root powers.

The ``configure_installation`` variant of the Spack package allows to control
the execution of the script. By negating the variant, Sarus can be installed as
a user different than root. It is then necessary to access the installation path
and run the configuration script directly. While doing so still requires
``sudo``, it can be performed with a user's personal Spack software and the
amount of root-owned files in the Sarus installation is kept to a minimum.

Installing as non-root likely means that Sarus is located in a directory
hierarchy which is not root-owned all the way up to ``/``.
This is not allowed by the program's :ref:`security checks
<post-installation-permissions-security>`, so they have to be disabled.

.. important::

    The ability to disable security checks is only meant as a convenience
    feature when rapidly iterating over test and development installations.
    It is strongly recommended to keep the checks enabled for production
    deployments.

Sarus can then be loaded and used normally. The procedure is summed up in the
following example:

.. code-block:: bash

   # Setup Spack bash integration (if you haven't already done so)
   . ${SPACK_ROOT}/share/spack/setup-env.sh

   # Create a local Spack repository for Sarus-specific dependencies
   export SPACK_LOCAL_REPO=${SPACK_ROOT}/var/spack/repos/cscs
   spack repo create ${SPACK_LOCAL_REPO}
   spack repo add ${SPACK_LOCAL_REPO}

   # Import Spack packages for Cpprestsdk, RapidJSON and Sarus
   cp -r <Sarus project root dir>/spack/packages/* ${SPACK_LOCAL_REPO}/packages/

    # Install Sarus skipping the configure_installation script
    $ spack install --verbose sarus@develop ~configure_installation

    # Use 'spack find' to get the installation directory
    $ spack find -p sarus@develop
    ==> 1 installed package
    -- linux-ubuntu18.04-sandybridge / gcc@7.5.0 --------------------
    sarus@develop  /home/ubuntu/spack/opt/spack/linux-ubuntu18.04-sandybridge/gcc-7.5.0/sarus-develop-dy4f6tlhx3vwf6zmqityvgdhpnc6clg5

    # cd into the installation directory
    $ cd /home/ubuntu/spack/opt/spack/linux-ubuntu18.04-sandybridge/gcc-7.5.0/sarus-develop-dy4f6tlhx3vwf6zmqityvgdhpnc6clg5

    # Run configure_installation script
    $ sudo ./configure_installation.sh

    # Disable security checks
    $ sudo sed -i -e 's/"securityChecks": true/"securityChecks": false/' etc/sarus.json

    # Return to home folder and test Sarus
    $ cd
    $ spack load sarus@develop
    $ sarus pull alpine
    [...]
    $ sarus run alpine cat /etc/os-release
    NAME="Alpine Linux"
    ID=alpine
    VERSION_ID=3.11.5
    PRETTY_NAME="Alpine Linux v3.11"
    HOME_URL="https://alpinelinux.org/"
    BUG_REPORT_URL="https://bugs.alpinelinux.org/"


.. _installation-source:

Installing from source
======================

Build and install
-----------------

Get Sarus source code:

.. code-block:: bash

    git clone git@github.com:eth-cscs/sarus.git
    cd sarus

Create a new folder ``${build_dir}}`` to build Sarus from source. e.g. ``build-Release``:

.. literalinclude:: ../../CI/utility_functions.bash
   :language: bash
   :start-after: # DOCS: Create a new folder
   :end-before: # DOCS: Configure and build

Configure and build (in this example ``${build_type} = Release`` and ``${toolchain_file} = gcc.cmake``:

.. literalinclude:: ../../CI/utility_functions.bash
   :language: bash
   :start-after: # DOCS: Configure and build
   :end-before: # DOCS: EO Configure and build

.. note::
    CMake should automatically find the dependencies (include directories,
    shared objects, and binaries). However, should CMake not find a dependency,
    its location can be manually specified through the command line. E.g.::

       cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake \
             -DCMAKE_INSTALL_PREFIX=${prefix_dir} \
             -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_PREFIX_PATH="<boost install dir>;<cpprestsdk install dir>;<libarchive install dir>;<rapidjson install dir>" \
             -Dcpprestsdk_INCLUDE_DIR=<cpprestsdk include dir> \
             ..

.. note::
    Old versions of CMake might have problems finding Boost 1.65.0.
    We recommend to use at least CMake 3.10 in order to avoid compatibility issues.

Below are listed the Sarus-specific options that can be passed to CMake in
order to customize your build:

   - ``CMAKE_INSTALL_PREFIX``: installation directory of Sarus [/usr/local].
   - ``ENABLE_SSH``: build and install the SSH hook and custom SSH software to enable
     connections inside containers [TRUE].
   - ``ENABLE_TESTS_WITH_VALGRIND``: run each unit test through valgrind [FALSE].

Copy files to the installation directory:

.. literalinclude:: ../../CI/utility_functions.bash
   :language: bash
   :start-after: # DOCS: Install Sarus
   :end-before: # DOCS: Create OCI bundle dir

Create the directory in which Sarus will create the OCI
bundle for containers. The location of this directory is configurable at any time, as
described in the next section. As an example, taking default values:

.. literalinclude:: ../../CI/utility_functions.bash
   :language: bash
   :start-after: # DOCS: Create OCI bundle dir
   :end-before: # DOCS: Finalize installation

Finalize the installation
-------------------------

To complete the installation, run the :ref:`configuration script <configure-installation-script>`
located in the installation path.
This script needs to run with root privileges in order to set Sarus as a
root-owned SUID program:

.. literalinclude:: ../../CI/utility_functions.bash
   :language: bash
   :start-after: # DOCS: Finalize installation
   :end-before: # DOCS: EO Finalize installation

.. note::
   The configuration script will create a minimal working configuration.
   For enabling additional features, please refer to the
   :doc:`/config/configuration_reference`.

As suggested by the output of the script, you should also add Sarus to your ``PATH``.
Add a line like ``export PATH=/opt/sarus/bin:${PATH}`` to your ``.bashrc`` in
order to make the addition persistent.
