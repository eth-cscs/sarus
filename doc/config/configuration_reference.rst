****************************
Configuration file reference
****************************

Configuration file entries
==========================

.. _config-reference-securityChecks:

securityChecks (bool, REQUIRED)
-------------------------------
Enable/disable runtime security checks to verify that security critical files
are not tamperable by non-root users. Disabling this may be convenient when
rapidly iterating over test and development installations. It is strongly
recommended to keep these checks enabled for production deployments. Refer to
the section about :ref:`security requirements
<post-installation-permissions-security>` for more details about these checks.

Recommended value: ``true``

.. _config-reference-OCIBundleDir:

OCIBundleDir (string, REQUIRED)
-------------------------------
Absolute path to where Sarus will generate an OCI bundle, from which a container
will be created. An OCI bundle is composed by a *config.json* configuration file
and a directory which will form the root filesystem of the container. The
``OCIBundleDir`` directory must satisfy the :ref:`security requirements
<post-installation-permissions-security>` for critical files and directories.

Recommended value: ``/var/sarus/OCIBundleDir``

.. _config-reference-rootfsFolder:

rootfsFolder (string, REQUIRED)
-------------------------------
The name Sarus will assign to the directory inside the OCI bundle; this
directory will become the root filesystem of the container.

Recommended value: ``rootfs``

prefixDir (string, REQUIRED)
----------------------------
Absolute path to the base directory where Sarus has been installed.
This path is used to find all needed Sarus-specific utilities.

Recommended value: ``/opt/sarus/<version>``

.. _config-reference-hooksDir:

hooksDir (string, OPTIONAL)
----------------------------
Absolute path to the directory containing the `OCI hook JSON configuration files
<https://github.com/containers/libpod/blob/master/pkg/hooks/docs/oci-hooks.5.md>`_.
See :doc:`/config/configure_hooks`.
Sarus will process the JSON files in *hooksDir* and generate the configuration
file of the OCI bundle accordingly. The hooks will effectively be called by
the OCI-compliant runtime specified by :ref:`runcPath <config-reference-runcPath>`.

Recommended value: ``/opt/sarus/<version>/etc/hooks.d``

.. _config-reference-tempDir:

tempDir (string, REQUIRED)
--------------------------
Absolute path to the directory where Sarus will create a temporary folder
to expand layers when pulling and loading images

Recommended value: ``/tmp``

.. _config-reference-localRepositoryBaseDir:

localRepositoryBaseDir (string, REQUIRED)
-----------------------------------------
Absolute base path to individual user directories, where Sarus will create
(if necessary) and access local image repositories. The repositories will be
located in ``<localRepositoryBaseDir>/<user name>/.sarus``.

Recommended value: ``/home``

.. _config-reference-centralizedRepositoryDir:

centralizedRepositoryDir (string, OPTIONAL)
-------------------------------------------
Absolute path to where Sarus will create (if necessary) and access the
system-wide centralized image repository. This repository is intended to satisfy
use cases where an image can only be broadcasted to the users, without allowing
them to pull the image directly (e.g. images that cannot be redistributed due to
licensing agreements).

The centralized repository is meant to be read-only for regular users, and its
contents should be modifiable only by the system administrators.

Recommended value: ``/var/sarus/centralized_repository``

mksquashfsPath (string, REQUIRED)
---------------------------------
Absolute path to trusted ``mksquashfs`` binary.
This executable must satisfy the :ref:`security requirements
<post-installation-permissions-security>` for critical files and directories.

.. _config-reference-initPath:

initPath (string, REQUIRED)
---------------------------------
Absolute path to trusted init process static binary which will launch the
user-specified applications within the container when the ``--init`` option
to :program:`sarus run` is used.
This executable must satisfy the :ref:`security requirements
<post-installation-permissions-security>` for critical files and directories.

By default, within the container Sarus only executes the user-specified application,
which is assigned PID 1. The PID 1 process has unique features in Linux:
most notably, the process will ignore signals by default and zombie processes
will not be reaped inside the container (see
`[1] <https://blog.phusion.nl/2015/01/20/docker-and-the-pid-1-zombie-reaping-problem/>`_ ,
`[2] <https://hackernoon.com/the-curious-case-of-pid-namespaces-1ce86b6bc900>`_ for further reference).

Running the container application through an init system provides a solution for
signaling container applications or reaping processes of long-running containers.

The standalone package of Sarus uses `tini <https://github.com/krallin/tini>`_ as its default init process.

.. warning::
   Some HPC applications may be subject to performance losses when run with an init process.
   Our internal benchmarking tests with `tini <https://github.com/krallin/tini>`_ showed
   overheads of up to 2%.

.. _config-reference-runcPath:

runcPath (string, REQUIRED)
---------------------------
Absolute path to trusted OCI-compliant runtime binary, which will be used by
Sarus to spawn the actual low-level container process.
This executable must satisfy the :ref:`security requirements
<post-installation-permissions-security>` for critical files and directories.

.. _config-reference-ramFilesystemType:

ramFilesystemType (string, REQUIRED)
------------------------------------
The type of temporary filesystem Sarus will use for setting up the base VFS
layer for the container. Must be either ``tmpfs`` or ``ramfs``.

A filesystem of this type is created inside a dedicated mount namespace unshared
by Sarus for each container. The temporary filesystem thus generated will be
used as the location of the OCI bundle, including the subsequent mounts (loop,
overlay and, if requested, bind) that will form the container's rootfs. The
in-memory and temporary nature of this filesystem helps with performance
and complete cleanup of all container resources once the Sarus process exits.

.. warning::
   When running on Cray Compute Nodes (CLE 5.2 and 6.0), ``tmpfs`` will not work
   and ``ramfs`` has to be used instead.

Recommended value: ``tmpfs``

.. _config-reference-siteMounts:

siteMounts (array, OPTIONAL)
----------------------------
List of JSON objects defining filesystem mounts that will be automatically
performed from the host system into the container bundle. This is typically
meant to make network filesystems accessible within the container but could be
used to allow certain other facilities.

Each object in the list must define the following fields:

* ``type`` (string): The type of the mount. Currently, only ``bind``
  (for bind-mounts) is supported.
* ``source`` (string): Absolute path to the host file/directory that
  will be mounted into the container.
* ``destination`` (string): Absolute path to where the filesystem will be made
  available inside the container.
  If the directory does not exist, it will be created.

Bind mounts
^^^^^^^^^^^
In addition to ``type``, ``source`` and ``destination``, bind mounts can optionally
add the following field:

* ``flags`` (object, OPTIONAL): Object defining the flags for the bind mount.
  Can have the following fields:

  - *readonly (string, empty value expected)*: Mount will be performed as
    read-only.

By default, bind mounts will always be of ``recursive private`` flavor. Refer to the
`Linux docs <https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt>`_
for more details.

General remarks
^^^^^^^^^^^^^^^
``siteMounts`` are not subject to the limitations of user mounts requested
through the CLI. More specifically, these mounts:

* Can specify any path in the host system as source
* Can specify any path in the container as destination

It is not recommended to bind things under ``/usr`` or other common critical
paths within containers.

It is OK to perform this under ``/var`` or ``/opt`` or a novel path that your
site maintains (e.g. ``/scratch``).

environment (object, OPTIONAL)
------------------------------
JSON object defining operations to be performed on the environment of the
container process. Can have four optional fields:

* ``set`` (array): List of JSON objects containing a single field, meant to
  represent the key-value pair of an environment variable. The variables defined
  here will be set in the container environment, possibly replacing any
  previously existing variables with the same names.
  Example::

      {"CONTAINER_ENVIRONMENT_VARIABLE": "1"}

  This can be useful to inform users applications and scripts that they are
  running inside a Sarus container.
* ``prepend`` (array): List of JSON objects containing a single field, meant to
  represent the key-value pair of an environment variable. The values will be
  prepended to the corresponding variables in the container. For example, this
  can be used to prepend site-specific locations to PATH.
* ``append`` (array): List of JSON objects containing a single field, meant to
  represent the key-value pair of an environment variable. The values will be
  appended to the corresponding variables in the container. For example, this
  can be used to append site-specific locations to PATH.
* ``unset`` (array): List of strings representing environment variable names.
  Variables with the corresponding names will be unset in the container.

userMounts (object, OPTIONAL)
-----------------------------
Normal users have to possibility of requesting custom paths available to them
in the host environment to be mapped to another path inside the container.
This is achieved through the ``--mount`` option of ``sarus run``.
The ``userMounts`` object offers the means to set limitations for this feature
through two arrays:

* ``notAllowedPrefixesOfPath``: list of strings representing starting paths.
  The user will not be able to enter these paths or any path under them as
  a mount destination. Default set to ``["/etc","/var","/opt/sarus"]``.

* ``notAllowedPaths``: list of strings representing exact paths.
  The user will not be able to enter these paths as a mount destination.
  Default set to ``["/opt"]``.

Both these fields and ``userMounts`` itself are optional: remove them to lift
any restriction.

These limitations apply only to mounts requested through the command line;
Mounts entered through ``siteMounts`` are not affected by them.


Example configuration file
==========================

.. code-block:: json

    {
        "securityChecks": true,
        "OCIBundleDir": "/var/sarus/OCIBundleDir",
        "rootfsFolder": "rootfs",
        "prefixDir": "/opt/sarus/1.0.0",
        "hooksDir": "/opt/sarus/1.0.0/etc/hooks.d",
        "tempDir": "/tmp",
        "localRepositoryBaseDir": "/home",
        "centralizedRepositoryDir": "/var/sarus/centralized_repository",
        "mksquashfsPath": "/usr/sbin/mksquashfs",
        "runcPath": "/usr/local/sbin/runc.amd64",
        "ramFilesystemType": "tmpfs",
        "siteMounts": [
            {
                "type": "bind",
                "source": "/home",
                "destination": "/home",
                "flags": {}
            }
        ],
        "environment": {
            "set": [
                {"VAR_TO_SET_IN_CONTAINER": "value"}
            ],
            "prepend": [
                {"VAR_WITH_LIST_OF_PATHS_IN_CONTAINER": "/path/to/prepend"}
            ],
            "append": [
                {"VAR_WITH_LIST_OF_PATHS_IN_CONTAINER": "/path/to/append"}
            ],
            "unset": [
                "VAR_TO_UNSET_IN_CONTAINER_0",
                "VAR_TO_UNSET_IN_CONTAINER_1"
            ]
        },
        "userMounts": {
            "notAllowedPrefixesOfPath": [
                "/etc",
                "/var",
                "/opt/sarus"
            ],
            "notAllowedPaths": [
                "/opt"
            ]
        },
        "OCIHooks": {
            "prestart": [
                {
                    "path": "/opt/sarus/1.0.0/bin/mpi_hook",
                    "env": [
                        "LDCONFIG_PATH=/sbin/ldconfig",
                        "MPI_LIBS=/usr/lib64/mvapich2-2.2/lib/libmpi.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpicxx.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpifort.so.12.0.5",
                        "MPI_DEPENDENCY_LIBS=",
                        "BIND_MOUNTS="
                    ]
                },
                {
                    "path": "/opt/sarus/1.0.0/bin/nvidia-container-runtime-hook.amd64",
                    "args": ["/opt/sarus/1.0.0/bin/nvidia-container-runtime-hook.amd64", "prestart"],
                    "env": [
                        "PATH=/usr/local/libnvidia-container_1.0.0-rc.2/bin",
                        "LD_LIBRARY_PATH=/usr/local/libnvidia-container_1.0.0-rc.2/lib"
                    ]
                }
            ]
        }
    }
