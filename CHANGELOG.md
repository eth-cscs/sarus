# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).


## [1.1.0] - 2020-02-03

### Added

- Added the `--workdir` option to `sarus run` for setting the initial working directory inside the container.
- Added "Communications" and "Publications" section to project README.
- Added documentation about complementing Sarus with Skopeo for interacting with 3rd party registries.
- Added integration tests for security checks.

### Changed

- Updated libarchive dependency to version 3.4.1.
- Updated recommended runc version to 1.0.0-rc10.
- Improved string parsing by using Boost functions.
- Site/user bind mounts have "recursive private" propagation by default. More details [here](https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt).
- Extensive code refactoring on the Native MPI hook:
    - Easier to extend and better control of performed actions.
    - More robust symlink generation.
    - Enhanced ABI version resolution.
    - Improved unit tests.
    - Factored out non-specific code to common utility functions.
- The Slurm global sync hook is activated only when the user requests activation of the SSH hook.
- Transitioned integration tests to Python 3 and pytest.
- Integration tests for the virtual cluster reuse the same Docker image of unit and integration tests.
- Updated cookbook page about the Intel Cluster Edition software.

### Deprecated

- Deprecated the use of the `bind-propagation` property for site/user bind mounts. It will be removed in a future release.

### Fixed

- Fixed propagation of CPU affinity from the host to the container process.
- Fixed some hyperlinks in the documentation

### Security

- Changes to security checks:
    - Reorganized and unified code for the checks.
    - Root ownership is checked based on uid, regardless of gid.
    - Root ownership for directories is checked recursively all the way up to the `/` directory.
    - Always check that `sarus.json` is untamperable regardless of the value of the configuration parameter.
- Improved usage of libarchive to prevent image contents from spilling outside of the expansion directory when extracting layers.


## [1.0.1] - 2019-11-08

### Security

- The SSH hook runs sshd as an unprivileged process.


## [1.0.0] - 2019-11-04

### Added

- Added the possibility to build Sarus as a static standalone binary.
- The CI generates a standalone archive which packages a static binary of Sarus, the hooks and binary dependencies. This archive can be used to quickly deploy Sarus.
- Added a script to configure a Sarus installation regardless of the installation method. The script automatically sets up Sarus for basic functionality. Advanced features can be enabled by editing the configuration file.
- Added a mechanism to preserve PMI2 file descriptors from the host into the container.
- Sarus' stdout and stderr file descriptors are duplicated and exposed to OCI hooks. The glibc hook reuses these file descriptors to print messages about its activation.
- Added the possibility to start an init process within the container with the `--init` option to `sarus run`. An init process is useful for handling signals or reaping zombie processes within containers (see the User guide for further details). Sarus uses [tini](https://github.com/krallin/tini) as its default init process.
- Added support for Travis CI.
- Added a Spack package for Sarus.
- Added a Quickstart documentation page.
- Added CI tests to verify installation from source on various Linux distributions. The scripts for these tests are also reused to generate code snippets in the documentation.

### Changed

- Errors generating from incorrect CLI usage print error messages instead of exception traces. Traces are still displayed when using `--verbose` or `--debug` options.
- `/dev/shm` is bind mounted from the host instead of having the OCI runtime create a new filesystem.
- Execution of security checks is now controlled through a parameter on the configuration file rather than a CMake option.
- Restored use of `pivot_root()` by runc.
- Improved output format and clarity of `sarus --help`.
- Changes to Sarus SSH key generation:
    - By default, `sarus ssh-keygen` generates ssh keys only if no keys already exist. `sarus ssh-keygen --overwrite` can be used to force the regeneration of keys.
    - Generation of keys is now protected by a lockfile to prevent race conditions.
- Updated recommended runc version to 1.0.0-rc9 (now also bundled in the standalone archive).
- Improved accuracy of test coverage data.
- Updated documentation for the NVIDIA Container Toolkit (formerly NVIDIA GPU hook).
- Refactored and streamlined documentation about installation and configuration procedures.

### Fixed

- Fixed files/directories generation in some situations by explicitly setting umask at the start of Sarus.
- Improved consistency of some integration tests.


## [1.0.0-rc7] - 2019-08-12

### Added

- Added the glibc hook: performs replacement of the container's glibc stack with an host counterpart. This ensures that resources injected from the host (e.g. MPI) work correctly on images which feature glibc versions too old for the host resources.
- Added cookbook page about the Intel Cluster Edition software
- Added support for publishing documentation on Read the Docs.

### Changed

- Extensive review and update of the cookbook.

### Fixed

- Native MPI hook: improved generation of symlinks in the container for more robust detection of the injected libraries by the dynamic linker.
- Print an error message when an unrecognized CLI global option is detected.


## [1.0.0-rc6] - 2019-05-07

### Added

- Updated license and applied license header to source files.
- Added the Timestamp hook and related documentation.

### Changed

- Container processes now inherit supplementary gids from the host process that called Sarus.
- The SSH hook build scripts now compile OpenSSL with a single process. Multi-process building was causing the process to fail on some Linux flavors.
- The Slurm global sync hook doesn't perform any action if Slurm environment variables are not defined in the container.
- Updated recommended runc version to 1.0.0-rc8.

### Removed

- Removed explicit documentation about building runc from source. A link to the related section in the official runc project is still present.

### Fixed

- Various fixes to documentation.


## [1.0.0-rc5] - 2019-03-08

### Fixed

- Fixed validation of destination paths for site/user mounts.


## [1.0.0-rc4] - 2019-03-04

### Fixed

- Fixed application of whiteouts during extraction of image layers.


## [1.0.0-rc3] - 2019-02-21

### Added

- New documentation content:
    - Section about building runc from source
    - Page about Slurm global sync hook
    - User doc about initial working directory in the container
    - Developer doc about running unit and integration tests
- CI checks to verify documentation can be built whether the source directory is a git repository or not.
- CI checks to verify correct detection of version string.
- CI checks to verify VERSION file is in sync with latest git tag.
- Cookbook example about OpenMPI through SSH.

### Changed

- Explicitly set permissions of the OCI bundle directory
- Enabled security checks when performing integration tests.
- Updated documentation about security checks.
- The SSH hook is built by default when building Sarus.

### Fixed

- Fixed extraction of image layers containing directories without the executable bit set.
- Various fixes to documentation.

### Security

- Updated recommended runc version to commit 6635b4f0c6 (addresses CVE-2019-5736).


## [1.0.0-rc2] - 2019-01-28

### Added

- Cookbook with use cases as part of the documentation.

### Changed

- Improved output of `sarus images`: includes server name in the repository string if the image is not from Docker Hub.
- Improved help messages of `sarus run`, `sarus rmi`: clearly state that the image repository must match the value displayed by `sarus images`.

### Fixed

- Fixed the `--version` global option.
- Pass the `--no-pivot` option to runc to avoid mount problems on CLE6.

### Security

- Security checks recursively verify parent directories and writable permissions of group or others.


## [1.0.0-rc1] - 2018-11-28

Initial release.

