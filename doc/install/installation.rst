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
<post-installation-permissions-execution>`, Sarus must be installed as a root-owned
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

Configure and build (in this example ``${build_type}`` is ``Release``:

.. literalinclude:: ../../CI/utility_functions.bash
   :language: bash
   :start-after: # DOCS: Configure and build
   :end-before: # DOCS: Install Sarus

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
   - ``ENABLE_SSH``: build and install the SSH hook and custom OpenSSH software to enable
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
