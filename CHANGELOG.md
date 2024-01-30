# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed

- Changed the implementation of the lock for the image repository metadata file to a mechanism based on flock(2).
  The new implementation can support both shared locks (a.k.a. read locks) and exclusive locks (a.k.a. write locks),
  and improves the startup time when launching large numbers of containers at scale.
- Updated CI distributed tests to use Docker Compose V2 and Compose file format version 3
- Updated automatic documentation build to use Sphinx 7.2.6 and Sphinx RTD Theme 2.0.0

## [1.6.2]

### Added
- SSH Hook: added support for the `com.hooks.ssh.pidfile_container` OCI annotation, which allows to customize the path to the Dropbear daemon PIDfile inside the container.
- SSH Hook: added support for the `com.hooks.ssh.pidfile_host` OCI annotation, which optionally copies the PIDfile of the Dropbear server to the specified path on the host.
- SSH Hook: added support for the `OVERLAY_MOUNT_HOME_SSH` environment variable, which allows to control the creation of an overlay filesystem on top of the container's `${HOME}/.ssh` directory.
  More details [here](https://sarus.readthedocs.io/en/stable/config/ssh-hook.html#hook-configuration)

## [1.6.1]

### Added

- SSH Hook: added support for the `com.hooks.ssh.authorize_ssh_key` OCI annotation, which allows to authorize a user-provided public key for connecting to the running container.
- Added a User Guide section about using Visual Studio Code's Remote Development extension in conjunction with Sarus and the SSH hook. More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#remote-development-with-visual-studio-code)

### Changed

- The configuration files for the SSH hook and the Slurm sync hook are no longer generated automatically as part of the CMake installation process.
  In other words, the aforementioned hooks are no longer configured and enabled by default.
- Updated recommended runc version to 1.1.9
- Updated CI tests from source on Fedora (36 -> 38) and OpenSUSE Leap (15.4 -> 15.5)

### Fixed

- Fixed support for image manifests which are provided by registries as multi-line, not indented JSON
- Fixed parsing from the command line of image references which feature registry host and image name, but no namespaces (e.g. `<registry>/<image>`)

### Security

- The installation directory of Sarus binaries is now always verified by the security checks.
  Previously the check on this directory could be skipped if no Sarus hooks were configured and if the runc and init binaries were located elsewhere.


## [1.6.0]

### Added

- Added the `sarus hooks` command to list the hooks configured for the engine
- Added the `--annotation` option to `sarus run` for setting custom annotations in the OCI bundle. More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#setting-oci-annotations)
- Added the `--mpi-type` option to `sarus run` for selecting an MPI hook among those configured by the system administrator
- Added a warning message when acquisition of a lock file on the local repository metadata file is taking an unusually long time.
  The message is displayed at a [configurable interval](https://sarus.readthedocs.io/en/stable/config/configuration_reference.html#repositorymetadatalocktimings-object-optional) (default 10 seconds), until the lock acquisition timeout is reached.
- Added support for the optional `defaultMPIType` parameter in the `sarus.json` configuration file. More details [here](https://sarus.readthedocs.io/en/stable/config/configuration_reference.html#defaultmpitype-string-optional).
- Added support for the optional `repositoryMetadataLockTimings` parameter in the `sarus.json` configuration file. More details [here](https://sarus.readthedocs.io/en/stable/config/configuration_reference.html#repositorymetadatalocktimings-object-optional).
- Added the AMD GPU OCI hook to provide access to ROCm AMD GPU devices inside the container. More details [here](https://sarus.readthedocs.io/en/stable/config/amdgpu-hook.html)
- Added a new OCI hook to perform arbitrary sequences of bind mounts and device mounts into containers.
  The hook is meant to streamline the implementation and usage of advanced features which can be enabled through sets of related mounts.
  More details [here](https://sarus.readthedocs.io/en/stable/config/mount-hook.html).
- Added a note about the Boost minimum required version 1.77 when building on ARM.

### Changed

- Sarus will now exit with an error if an operation requiring a lock file on the local repository metadata cannot acquire a lock within the [configured timeout duration](https://sarus.readthedocs.io/en/stable/config/configuration_reference.html#repositorymetadatalocktimings-object-optional) (default 60 seconds).
  Previously, Sarus would keep attempting to acquire a lock indefinitely.
- When printing error traces, entries related to standard C++ exceptions now provide clearer information
- Updated recommended runc version to 1.1.6
- Updated recommended libnvidia-container version to 1.13.0
- Updated recommended NVIDIA Container Toolkit version to 1.13.0

### Fixed

- Fixed a race condition when pulling private images concurrently with the same user
- Fixed a bug which was causing repository metadata files and their corresponding lockfiles to be created or atomically updated with root group ownership after executing a `sarus run` command.
  The aforementioned files are now correctly created or updated with user and group ownership of the user who launched Sarus.


## [1.5.2]

### Added

- Added support for passing command-line options to `mksquashfs` through the `mksquashfsOptions` parameter in the `sarus.json` configuration file
- Added explicit forwarding of standard signals from engine to OCI runtime
- Added experimental support for the PMIx v3 interface. Given its experimental nature, this feature has to be enabled through a parameter in the `sarus.json` configuration file
- Added CI unit and integration tests from source on Rocky Linux 8 and 9

### Changed

- The `sarus run` and `sarus images` commands now automatically remove images missing the internal SquashFS or metadata file, and report them as not available
- The MPI hook and Glibc hook no longer enter the container PID namespace
- The Slurm Global Sync hook and the Timestamp hook no longer enter any container namespace
- Updated recommended runc version to 1.1.3
- Updated recommended libnvidia-container version to 1.11.0
- Updated recommended NVIDIA Container Toolkit version to 1.11.0
- Updated documentation about the NVIDIA Container Toolkit to refer more specifically to the NVIDIA Container Runtime hook
- The `configure_installation.sh` script can now acquire custom values for the local and/or centralized repository paths from environment variables.
  More details [here](https://sarus.readthedocs.io/en/stable/config/basic_configuration.html#using-the-configure-installation-script)
- Updated CI tests from source on Ubuntu (21.10 -> 22.04), Fedora (35 -> 36) and OpenSUSE Leap (15.3 -> 15.4)

### Removed

- Removed CI tests from source on Ubuntu 20.04 and CentOS 7

### Security

- The executable pointed by the `mksquashfsPath` parameter in the `sarus.json` configuration file has been excluded from the security checks.
  The `mksquashfs` utility is only used by `sarus pull` and `sarus load` commands, which already run without privileges


## [1.5.1]

### Changed

- Changed the default registry to `docker.io`.
  When the server is not entered as part of the image reference, the `sarus run` command first looks under `docker.io` repositories and, if the image is not available, falls back to images under the previous default server (`index.docker.io`). This is done to preserve compatibility with existing workflows.
  The `sarus images` and `sarus rmi` commands treat images from `index.docker.io` as images from a 3rd party registry.
- If the image manifest obtained from a registry during a pull does not feature the `mediaType` property, Sarus now attempts to process the manifest as an OCI Manifest V1 instead of failing with an error.
- Updated recommended libnvidia-container version to 1.10.0
- Updated recommended NVIDIA Container Toolkit version to 1.10.0
- Replaced Travis public CICD with Github Actions

### Fixed

- Fixed an issue in the generation of manifest digests, where the digest result was incorrectly influenced by JSON formatting
- Fixed an inconsistency with Skopeo which was preventing to pull private images from Docker Hub


## [1.5.0]

### Added

- Added [Skopeo](https://github.com/containers/skopeo) as a dependency to pull or load container images
- Added [Umoci](https://umo.ci/) as a dependency to unpack OCI images
- Added support for pulling, running and removing images by digest
- Added the `--digests` option to `sarus images` for displaying digests of locally available images
- Added the `--username` and `--password-stdin` options to `sarus pull` for supplying authentication credentials directly on the command line.
  More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#pulling-images-from-private-repositories)
- Added support for the optional `containersPolicy` parameter in the `sarus.json` configuration file.
  More details [here](https://sarus.readthedocs.io/en/stable/config/configuration_reference.html#containerspolicy-object-optional).
- Added support for the optional `containersRegistries.dPath` parameter in the `sarus.json` configuration file.
  More details [here](https://sarus.readthedocs.io/en/stable/config/configuration_reference.html#containersregistries-dpath-string-optional).
- Added support for labels defined in OCI image configurations
- Added CI unit and integration tests from source on Ubuntu 21.10, Debian 11 and Fedora 35
- Added git submodule for RapidJSON (commit fcb23c2dbf) to simplify dependency management and build process

### Changed

- The `sarus images` command now displays the image ID by default.
  The image ID, as defined by the OCI Image Specification, is the hash of the image's configuration JSON. More details [here](https://github.com/opencontainers/image-spec/blob/main/config.md#imageid)
- The `sarus pull` command now skips the pull if the requested image is already available locally and up-to-date
- zlib is no longer a dependency of Sarus itself, but remains a dependency of the Dropbear software used by the SSH hook
- Updated the build environment of the Sarus static standalone package to Alpine Linux 3.15

### Removed

- Removed the `insecureRegistries` parameter from `sarus.json` and the built-in support for insecure registries.
  Access to insecure registries via Skopeo must now be enabled through containers-registries.conf(5) files.
  More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#pulling-images-from-insecure-registries)
- Removed dependencies on cpprestsdk, libarchive, OpenSSL, libcap, and libexpat
- Removed CI unit and integration tests from source on Ubuntu 18.04, Debian 10 and Fedora 34


## [1.4.2]

### Changed

- The Glibc hook now uses the output of `ldd` to detect the version of glibc
- Sarus now attempts to parse the Bearer authorization token regardless of the value of the `Content-Type` response header when pulling images


## [1.4.1]

### Added

- Added support for proxy connections when pulling images from remote registries
- Added CMake option to control build of unit test executables

### Changed

- Updated recommended runc version to 1.0.3
- Updated recommended libnvidia-container version to 1.7.0
- Updated recommended NVIDIA Container Toolkit version to 1.7.0
- Updated CppUTest framework for unit tests to version 4.0

### Fixed

- Fixed generation of README files for standalone archives


## [1.4.0]

### Added

- Added the ability to pull from insecure registries via `insecureRegistries` parameter in `sarus.json`
- Added the `-e/--env` option to `sarus run` for setting environment variables inside the container. More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#environment)
- Added the `--device` option to `sarus run` for mounting and whitelisting devices inside containers. More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#mounting-custom-devices-into-the-container)
- Added support for the optional `siteDevices` parameter in the `sarus.json` configuration file.
  This parameter can be used by administrators for defining devices to be automatically mounted and whitelisted inside containers.
- Added the `--pid` option to `sarus run` for setting the container PID namespace. More details [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#pid-namespace)
- Added support for applying seccomp profiles to containers
- Added support for applying AppArmor profiles to containers
- Added support for applying SELinux labels to container processes and to mounts performed by the OCI runtime
- The MPI hook whitelists access to devices bind mounted inside containers
- cgroup filesystems are mounted inside containers
- Added script to check for host requirements in CI, linked in documentation.
- Added CI unit and integration tests from source on Fedora 34 and OpenSUSE Leap 15.3

### Changed

- Containers now use the host's PID namespace by default. A private PID namespace can be requested through the CLI
- The `--ssh` option of `sarus run` now implies `--pid=private`
- Changed format of the `environment` parameter in the `sarus.json` configuration file
- Updated documentation about how the initial environment variables are set in containers
- Updated recommended Boost version to 1.77.0
- Updated recommended Cpprestsdk version to 2.10.18
- Updated recommended libarchive version to 3.5.2
- Updated recommended RapidJSON version to commit 00dbcf2
- Updated recommended runc version to 1.0.2
- Updated recommended libnvidia-container version to 1.5.1
- Updated recommended NVIDIA Container Toolkit version to 1.5.1
- Updated Dropbear software used by the SSH hook to version 2020.81
- Miscellaneous updates to Dockerfiles used for CI stages; in particular, the Sarus static standalone package is now built on Alpine Linux 3.14 with a GCC 10.3.1 toolchain

### Fixed

- Corrected the error message when attempting to pull an image by digest

### Removed

- The use of the `bind-propagation` property for bind mounts (deprecated in Sarus 1.1.0) has now been removed. All bind mounts are done with recursive private (`rprivate`) propagation.

### Security

- Access to custom devices within containers is not allowed by default


## [1.3.3]

### Added

- Added CI unit and integration tests from source on Ubuntu 20.04
- Added regular cleanups of CI caches on GitLab
- Added diagrams representing CI/CD workflows to developer documentation
- Added Markdown builder for Sphinx documentation

### Changed

- Updated minimum required CMake version to 2.8.12
- Improved clarity of some messages from the MPI hook
- Updated copyright notice and license formatting
- Migrated container images used by unit and integration tests to Quay.io

### Fixed

- Fixed bug preventing extraction of image layers with hardlinks pointing to absolute paths
- Small fix to RapidJSON installation documentation


## [1.3.2]

### Added

- Added `CONTRIBUTING.md` file with guidelines about contributing to the project
- Added CI tests for the Spack package on Ubuntu 18.04, Debian 10, CentOS 7, Fedora 31, OpenSUSE Leap 15.2
- Added `wget` and `autoconf` as buildtime dependencies in the Spack package
- Added a documentation note about compiler selection when installing on CentOS 7 using the Spack package
- Added a documentation note about installing the static version of the glibc libraries when installing using the Spack package

### Fixed

- Fixed a bug preventing bind mounts to `/dev` in the container

### Removed

- Removed the CI test for the Spack package on Ubuntu 16.04


## [1.3.1]

### Added

- Support for pulling images from registries which do not use content redirect for blobs

### Fixed

- Fixed extraction of image layers when replacing directories with other file types
- MPI and Glibc hooks skip entries from the dynamic linker cache if such entries do not exist
  in the container's filesystem

### Security

- Slurm global sync hook drops privileges at startup
- MPI and Glibc hooks now perform validations with user credentials for host mounts and writes


## [1.3.0]

### Added

- Customizable sarus and hooks configuration templates within etc folder
- Port number used by the SSH hook is now configurable
- Added note in the User Guide about bind mounting FUSE filesystems into Sarus containers

### Changed

- The OCI hooks are now configured through [OCI hook JSON configuration files](https://github.com/containers/libpod/blob/master/pkg/hooks/docs/oci-hooks.5.md). The previous OCI hooks configuration through `sarus.json` is no longer supported and Sarus Administrators should reconfigure their hooks according to the Sarus' [hook documentation page](https://sarus.readthedocs.io/en/stable/config/configure_hooks.html)
- Replaced the custom OpenSSH used by the SSH hook with Dropbear
- Made CPU affinity detection more robust
- Updated recommended tini version to 0.19.0
- Updated recommended libnvidia-container version to 1.2.0
- Updated recommended NVIDIA Container Toolkit version to 1.2.1

### Fixed

- CLI: fixed detection of option values separated by whitespace
- CLI: 'sarus run' does not return an error anymore when passing an option (i.e. a token starting with "-") as the first argument to the container application.
  This allows to directly pass options to containers which feature an entrypoint
- Support for root_squashed filesystems as image storage and as bind mounts sources
- When executing unit tests through the CTest program, tests now run in the directory of the test binary
- Fixed broken links in the documentation


## [1.2.0] - 2020-06-17

### Added

- Enabled Sarus to print log messages from the OCI Hooks
- Better documentation for ABI Compatibility [here](https://sarus.readthedocs.io/en/stable/user/abi_compatibility.html)
- Added User Guide section about running MPI applications without the MPI hook. See [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#running-mpi-applications-without-the-native-mpi-hook)
- Added documentation about requiring Linux kernel >= 3.0 and util-linux >= 2.20
- Added AddressSanitizer CI job

### Changed

- The glibc Hook is no longer activated by default, unless the `--mpi` option is used. To activate it explicitly, the new `--glibc` option of `sarus run` can be used. See [here](https://sarus.readthedocs.io/en/stable/user/user_guide.html#glibc-replacement)
- Using OCI annotations instead of environment variables to pass information to hooks. It is an internal change, transparent to users, moving towards OCI Hooks independence from Sarus
- Most of the Environment Variables for Hooks were renamed. Sarus Administrators should check the new names in the [respective hook documentation pages](https://sarus.readthedocs.io/en/stable/config/configure_hooks.html#hooks-use-cases)
- OCI MPI Hook will now enable MPI "backwards" library injections, issuing a warning. More details [here](https://sarus.readthedocs.io/en/stable/user/abi_compatibility.html)
- Improved the retrieval of image manifests from remote registries to better leverage the OCI Distribution specification
- Removed the explicit use of the `autoclear` option when loop-mounting squashfs images. Explicit use of the option causes a failure on
  Linux kernels >= 5.4. The `autoclear` option is still set implicitly by the `mount` system utility since June 2011 for kernels > 2.6.37.
- Updated Spack packages and installation instructions
- Updated documentation about the NVIDIA Container Toolkit. See [here](https://sarus.readthedocs.io/en/stable/config/gpu-hook.html)
- The SSH and Slurm global sync hooks now use configurable paths for their resources and are no longer dependant on Sarus-specific directories
- Reviewed and updated documentation about runtime security checks. See [here](https://sarus.readthedocs.io/en/stable/install/post-installation.html#security-related)
- Several improvements to the Continuous Integration workflow

### Fixed

- Fixed bug on OCI MPI Hook which failed to run containers having multiple versions of an MPI Dependency library
- Runtime security checks no longer fail if a checked path does not exist
- Fixed setting of default bind propagation values for custom mounts
- Fixed parsing of authentication challenges from the NVIDIA GPU Cloud registry
- Fixed the ability to pull images from the Quay.io registry

### Security

- Compiling now with -fstack-protector-strong as a measure against buffer overflows


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
