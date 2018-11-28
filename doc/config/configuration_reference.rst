****************************
Configuration file reference
****************************

The runtime configuration parameters of Sarus are read from a file called
*sarus.json*, which is expected to be a valid `JSON document
<https://www.json.org/>`_. Given the :ref:`privileges
<requirements-permissions>` that Sarus requires in order to exercise its
functions, as a security measure, the configuration file must be owned by root
and located in a root-owned directory. The location for the configuration file
can be set at build-time (see :doc:`/install/installation`).


Configuration file entries
==========================

.. _config-reference-OCIBundleDir:

OCIBundleDir (string, REQUIRED)
-------------------------------
Absolute path to where Sarus will generate an OCI bundle, from which a
container will be created. An OCI bundle is composed by a *config.json*
configuration file and a directory which will form the root filesystem
of the container. The ``OCIBundleDir`` directory must exist and be root-owned.

Recommended value: ``/var/sarus/OCIBundleDir``

.. _config-reference-rootfsFolder:

rootfsFolder (string, REQUIRED)
-------------------------------
The name Sarus will assign to the directory inside the OCI bundle; this
directory will become the root filesystem of the container.

Recommended value: ``rootfs``

prefixDir (string, REQUIRED)
-----------------------------
Absolute path to the base directory where Sarus has been installed.
This path is used to find all needed Sarus-specific utilities.

Recommended value: ``/opt/sarus/<version>``

dirOfFilesToCopyInContainerEtc (string, REQUIRED)
--------------------------
Absolute path to the directory containing the files that will be automatically
copied by Sarus into the container's /etc folder. Such files are:
hosts, resolv.conf, nsswitch.conf, passwd, group.

Recommended value: ``/opt/sarus/<version>/files_to_copy_in_container_etc``

.. _config-reference-tempDir:

tempDir (string, REQUIRED)
---------------------------
Absolute path to the directory where Sarus will create a temporary folder
to expand layers when pulling and loading images

Recommended value: ``/tmp``


.. _config-reference-localRepositoryBaseDir:

localRepositoryBaseDir (string, REQUIRED)
------------------------------------------
Absolute base path to individual user directories, where Sarus will create
(if necessary) and access local image repositories. The repositories will be
located in ``<localRepositoryBaseDir>/<user name>/.sarus``.

Recommended value: ``/home``

.. _config-reference-centralizedRepositoryDir:

centralizedRepositoryDir (string, OPTIONAL)
--------------------------------------------
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
It must be a root-owned executable in a root-owned directory.

runcPath (string, REQUIRED)
---------------------------
Absolute path to trusted OCI-compliant runtime binary, which will be used by
Sarus to spawn the actual low-level container process.
It must be a root-owned executable in a root-owned directory.

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
* ``destination`` (string): Absolute path to where the filesystem will be made
  available inside the container.
  If the directory does not exist, it will be created.

Bind mounts
^^^^^^^^^^^
Bind mount objects can specify the following fields:

* ``source`` (string, REQUIRED): Absolute path to the host file/directory that
  will be mounted into the container.
* ``flags`` (object, OPTIONAL): Object defining the flags for the bind mount.
  Can have the following fields:

  - *readonly (string, empty value expected)*: Mount will be performed as
    read-only.
  - *bind-propagation (string)*: Specifies the type of bind propagation to
    use for the mount. Can be one of ``recursive``, ``slave``, ``private``,
    ``rslave``, ``rprivate`` (the last two values stand for "recursive
    private" and "recursive slave" respectively).


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

.. _config-reference-OCIHooks:

OCIHooks (object, OPTIONAL)
---------------------------
Object defining the hooks that will be called to customize the container. Must
use the format indicated in :doc:`/config/configure_hooks`. This object will be
copied without modifications by Sarus into the configuration file of the
generated OCI bundle. The hooks will effectively be called by the OCI-compliant
runtime specified by :ref:`runcPath <config-reference-runcPath>`.


Example configuration file
==========================

.. code-block:: json

    {
        "OCIBundleDir": "/var/sarus/OCIBundleDir",
        "rootfsFolder": "rootfs",
        "prefixDir": "/opt/sarus",
        "dirOfFilesToCopyInContainerEtc": "/opt/sarus/files_to_copy_in_container_etc",
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
                    "path": "/opt/sarus/bin/mpi_hook",
                    "env": [
                        "SARUS_MPI_LIBS=/usr/lib64/mvapich2-2.2/lib/libmpi.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpicxx.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpifort.so.12.0.5",
                        "SARUS_MPI_DEPENDENCY_LIBS=",
                        "SARUS_MPI_BIND_MOUNTS=",
    		            "PATH=/usr/sbin"
                    ]
                },
                {
                    "path": "/opt/sarus/bin/nvidia-container-runtime-hook.amd64",
                    "args": ["/opt/sarus/bin/nvidia-container-runtime-hook.amd64", "prestart"],
                    "env": [
                        "PATH=/usr/local/libnvidia-container_1.0.0-rc.2/bin",
                        "LD_LIBRARY_PATH=/usr/local/libnvidia-container_1.0.0-rc.2/lib"
                    ]
                }
            ]
        }
    }
