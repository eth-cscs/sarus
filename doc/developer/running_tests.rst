**********************************
Running unit and integration tests
**********************************

Unit tests
==========

In order to run Sarus unit tests, it is advised to build Sarus with runtime
security checks disabled:

.. code-block:: bash

   $ cd <sarus project root dir>
   $ mkdir build && cd build
   $ cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake \
           -DCMAKE_INSTALL_PREFIX=/opt/sarus \
           ..
   $ make

Disabling security checks prevents some tests from failing because some files
(e.g. artifacts to test JSON parsing and validation) are not root-owned and
located in root-owned directories. The unit tests for security checks
individually re-enable the functionality to reliably verify its effectiveness.

The unit tests are written using the `CppUTest <https://cpputest.github.io/>`_
framework, which the build scripts are able to retrieve and compile
automatically.

The tests are run from the build directory with the help of CTest, the test
driver program from the CMake suite. We differentiate between normal tests and
test that require root privileges (e.g. those performing filesystem mounts). The
latter are identified by the suffix ``AsRoot`` in the name of the test
executable.

.. code-block:: bash

   # Run normal unit tests
   $ CTEST_OUTPUT_ON_FAILURE=1 ctest --exclude-regex 'AsRoot'

   # Run 'AsRoot' unit tests
   $ sudo CTEST_OUTPUT_ON_FAILURE=1 ctest --tests-regex 'AsRoot'


Generating coverage data
------------------------

If the build was configured with the CMake ``ENABLE_TESTS_WITH_GCOV`` enabled,
the unit tests executables automatically generate ``gcov`` files with raw
coverage data. We can process and summarize these data using the `gcovr <https://gcovr.com/>`_
utility:

.. note::

   To yield reliable results, it is advised to collect unit test coverage data
   only when the build has been performed in "Debug" configuration.

.. code-block:: bash

   # Command general form
   $ gcovr -r <sarus project root dir>/src -k -g --object-directory <build dir>/src

   # Assuming that we are in the build directory and that the project root
   # is the parent directory
   $ gcovr -r ../src -k -g --object-directory $(pwd)/src


Integration tests
=================

Integration tests use Python 2.7 and the packages indicated in the
:ref:`Requirements page <requirements-packages>`. Sarus must be correctly
installed and configured on the system in order to successfully perform
integration testing. Before running the tests, we need to re-target the
centralized repository to a location that is writable by the current user (this
is not necessary if running integration tests as root):

.. code-block:: bash

   $ mkdir -p ~/sarus-centralized-repository
   $ sudo sed -i -e 's@"centralizedRepositoryDir": *".*"@"centralizedRepositoryDir": "/home/docker/sarus-centralized-repository"@' /opt/sarus/etc/sarus.json

.. note::

   Integration tests are not exposed to the risk of failing when runtime security
   checks are enabled, like unit tests are. To test a configuration more similar
   to a production deployment, re-build and install Sarus with runtime security checks
   enabled.

We can run the tests from the parent directory of the related Python scripts:

.. code-block:: bash

   $ cd  <sarus project root dir>/CI/src
   $ PYTHONPATH=$(pwd):$PYTHONPATH CMAKE_INSTALL_PREFIX=/opt/sarus/ nosetests -v integration_tests/test*.py
