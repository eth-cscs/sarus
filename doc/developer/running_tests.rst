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
           -DENABLE_RUNTIME_SECURITY_CHECKS=FALSE \
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

      Test project /home/docker/sarus/build
            Start  1: test_common_Logger
       1/16 Test  #1: test_common_Logger .................   Passed    0.02 sec
            Start  2: test_common_Error
       2/16 Test  #2: test_common_Error ..................   Passed    0.02 sec
            Start  3: test_common_ImageMetadata
       3/16 Test  #3: test_common_ImageMetadata ..........   Passed    0.01 sec
            Start  4: test_common_Lockfile
       4/16 Test  #4: test_common_Lockfile ...............   Passed    0.02 sec
            Start  5: test_common_JSON
       5/16 Test  #5: test_common_JSON ...................   Passed    0.01 sec
            Start  6: test_common_CLIArguments
       6/16 Test  #6: test_common_CLIArguments ...........   Passed    0.01 sec
            Start  7: cli_CLI
       7/16 Test  #7: cli_CLI ............................   Passed    0.08 sec
            Start  8: cli_Utility
       8/16 Test  #8: cli_Utility ........................   Passed    0.04 sec
            Start  9: cli_MountParser
       9/16 Test  #9: cli_MountParser ....................   Passed    0.05 sec
            Start 10: test_image_manager_LoadedImage
      10/16 Test #10: test_image_manager_LoadedImage .....   Passed    0.09 sec
            Start 11: test_image_manager_PulledImage
      11/16 Test #11: test_image_manager_PulledImage .....   Passed    2.32 sec
            Start 12: test_image_manager_SquashfsImage
      12/16 Test #12: test_image_manager_SquashfsImage ...   Passed    0.04 sec
            Start 13: test_image_manager_ImageStore
      13/16 Test #13: test_image_manager_ImageStore ......   Passed    0.04 sec
            Start 14: test_image_manager_Puller
      14/16 Test #14: test_image_manager_Puller ..........   Passed    2.10 sec
            Start 15: runtime_OCIBundleConfig
      15/16 Test #15: runtime_OCIBundleConfig ............   Passed    0.03 sec
            Start 16: runtime_ConfigsMerger
      16/16 Test #16: runtime_ConfigsMerger ..............   Passed    0.05 sec

      100% tests passed, 0 tests failed out of 16

      Total Test time (real) =   5.00 sec

   # Run 'AsRoot' unit tests
   $ sudo CTEST_OUTPUT_ON_FAILURE=1 ctest --tests-regex 'AsRoot'

      Test project /home/docker/sarus-build
          Start  1: test_common_Utility_AsRoot
      1/8 Test  #1: test_common_Utility_AsRoot ............   Passed    0.06 sec
          Start  8: test_common_SecurityChecks_AsRoot
      2/8 Test  #8: test_common_SecurityChecks_AsRoot .....   Passed    0.01 sec
          Start 17: runtime_MountUtilities_AsRoot
      3/8 Test #17: runtime_MountUtilities_AsRoot .........   Passed    0.08 sec
          Start 18: runtime_SiteMount_AsRoot
      4/8 Test #18: runtime_SiteMount_AsRoot ..............   Passed    0.09 sec
          Start 19: runtime_UserMount_AsRoot
      5/8 Test #19: runtime_UserMount_AsRoot ..............   Passed    0.09 sec
          Start 20: runtime_Runtime_AsRoot
      6/8 Test #20: runtime_Runtime_AsRoot ................   Passed    0.10 sec
          Start 23: hooks_mpi_MPIHook_AsRoot
      7/8 Test #23: hooks_mpi_MPIHook_AsRoot ..............   Passed    0.78 sec
          Start 24: hooks_slurm_global_sync_Hook_AsRoot
      8/8 Test #24: hooks_slurm_global_sync_Hook_AsRoot ...   Passed    0.05 sec

      100% tests passed, 0 tests failed out of 8

      Total Test time (real) =   1.28 sec


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
      ------------------------------------------------------------------------------
                              GCC Code Coverage Report
      Directory: ../src
      ------------------------------------------------------------------------------
      File                                       Lines    Exec  Cover   Missing
      ------------------------------------------------------------------------------
      cli/CLI.cpp                                   60      54    90%   [...]
      cli/CommandObjectsFactory.hpp                  1       0     0%   [...]
      cli/MountParser.cpp                          159     131    82%   [...]
      cli/MountParser.hpp                            1       0     0%   [...]
      cli/Utility.cpp                               60      52    86%   [...]
      common/Config.hpp                              1       1   100%
      common/Error.hpp                               8       8   100%
      common/ImageID.hpp                             1       0     0%   [...]
      common/ImageMetadata.hpp                       2       2   100%
      common/Logger.hpp                              1       1   100%
      common/SarusImage.hpp                          1       0     0%   [...]
      hooks/mpi/MpiHook.cpp                        148     141    95%   [...]
      hooks/slurm_global_sync/Hook.cpp              63      56    88%   [...]
      image_manager/ImageStore.cpp                 121     109    90%   [...]
      image_manager/InputImage.hpp                   1       0     0%   [...]
      image_manager/LoadedImage.cpp                 47      38    80%   [...]
      image_manager/PulledImage.cpp                 66      60    90%   [...]
      image_manager/Puller.cpp                     243     186    76%   [...]
      image_manager/SquashfsImage.cpp               24      22    91%   [...]
      runtime/ConfigsMerger.cpp                     59      57    96%   [...]
      runtime/ConfigsMerger.hpp                      1       0     0%   [...]
      runtime/Mount.hpp                              1       1   100%
      runtime/OCIBundleConfig.cpp                  158     154    97%   [...]
      runtime/OCIBundleConfig.hpp                    1       0     0%   [...]
      runtime/Runtime.cpp                          113      88    77%   [...]
      runtime/SiteMount.cpp                         23      18    78%   [...]
      runtime/UserMount.cpp                         49      34    69%   [...]
      runtime/mount_utilities.cpp                  103      72    69%   [...]
      ------------------------------------------------------------------------------
      TOTAL                                       1516    1285    84%
      ------------------------------------------------------------------------------


Integration tests
=================

Integration tests use Python 2.7 and the packages indicated in the :ref:`Requirements page <requirements-packages>`.
Sarus must be correclty installed and configured on the system in order to
successfully perform integration testing.
Before running the tests, we need to re-target the centralized repository to
a location that is writable by the current user (this is not necessary if
running integration tests as root):

.. code-block:: bash

   $ mkdir -p ~/sarus-centralized-repository
   $ sudo sed -i -e 's@"centralizedRepositoryDir": *".*"@"centralizedRepositoryDir": "/home/docker/sarus-centralized-repository"@' /opt/sarus/etc/sarus.json

.. note::

   Integration tests are not exposed to the risk of failing when runtime security
   checks are enabled, like unit tests do. To test a configuration more similar
   to a production deployment, re-build and install Sarus with runtime security checks
   enabled.

We can run the tests from the parent directory of the related Python scripts::

.. code-block:: bash

   $ cd  <sarus project root dir>/CI/src
   $ PYTHONPATH=$(pwd):$PYTHONPATH CMAKE_INSTALL_PREFIX=/opt/sarus/ nosetests -v integration_tests/test*.py
