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
---------------------------
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

.. _config-reference-skopeoPath:

skopeoPath (string, REQUIRED)
-----------------------------
Absolute path to a trusted ``skopeo`` binary, which will be used to pull images
from container registries or load them from local files.

umociPath (string, REQUIRED)
----------------------------
Absolute path to a trusted ``umoci`` binary, which will be used to unpack image
contents before converting them to SquashFS format.

mksquashfsPath (string, REQUIRED)
---------------------------------
Absolute path to trusted ``mksquashfs`` binary.
This executable must satisfy the :ref:`security requirements
<post-installation-permissions-security>` for critical files and directories.

.. _config-reference-initPath:

initPath (string, REQUIRED)
---------------------------
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

.. _config-reference-insecureRegistries:

insecureRegistries (array, OPTIONAL)
------------------------------------
List of strings defining registries for which TLS/SSL security will not be enforced
when pulling images. Note that this opens the door for many potential security
vulnerabilities, and as such should only be used in exceptional cases such as local
testing.

siteDevices (array, OPTIONAL)
-----------------------------
List of JSON object defining device files which will be automatically mounted
from the host filesystem into the container bundle. The devices will also be
whitelisted in the container's device cgroup (Sarus disables access to
custom device files by default).

Each object in the list supports the following fields:

* ``source`` (string, REQUIRED): Absolute path to the device file on the host.
* ``destination`` (string, OPTIONAL): Absolute path to the desired path for the
  device in the container. In the absence of this field, the device will be bind
  mounted at the same path within the container.
* ``access`` (string, OPTIONAL): Flags defining the the type of access the device will
  be whitelisted for. Must be a combination of the characters ``rwm``, standing
  for *read*, *write* and *mknod* access respectively; the characters may come
  in any order, but must not be repeated. Permissions default to ``rwm`` if this
  field is not present.

  As highlighted in the related :ref:`section of the User Guide <user-device-mounts>`,
  Sarus cannot grant more access permissions than those allowed in the host
  device cgroup.

.. _config-reference-environment:

environment (object, OPTIONAL)
------------------------------
JSON object defining operations to be performed on the environment of the
container process. Can have four optional fields:

* ``set`` (object): JSON object with fields having string values. The fields
  represent the key-value pairs of environment variables. The variables defined
  here will be set in the container environment, possibly replacing any
  previously existing variables with the same names.
  This can be useful to inform users applications and scripts that they are
  running inside a Sarus container.
* ``prepend`` (object): JSON object with fields having string values. The fields
  represent the key-value pairs of environment variables. The values will be
  prepended to the corresponding variables in the container, using a colon as
  separator. This can be used, for example, to prepend site-specific locations
  to PATH.
* ``append`` (object): JSON object with fields having string values. The fields
  represent the key-value pairs of environment variables. The values will be
  appended to the corresponding variables in the container, using a colon as
  separator. This can be used, for example, to append site-specific locations
  to PATH.
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

seccompProfile (string, OPTIONAL)
---------------------------------
Absolute path to a file defining a seccomp profile in accordance with the
`JSON format specified by the OCI Runtime Specification
<https://github.com/opencontainers/runtime-spec/blob/master/config-linux.md#seccomp>`_.
This profile will be applied to the container process by the OCI runtime.

`Seccomp <https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt>`_
(short for "SECure COMPuting mode") is a Linux kernel feature allowing
to filter the system calls which are performed by a given process.
It is intended to minimize the kernel surface exposed to an application.

For reference, you may refer to the default seccomp profiles used by
`Docker <https://github.com/moby/moby/blob/master/profiles/seccomp/default.json>`_,
`Singularity CE <https://github.com/hpcng/singularity/blob/master/etc/seccomp-profiles/default.json>`_
or `Podman <https://github.com/containers/common/blob/main/pkg/seccomp/seccomp.json>`_.

apparmorProfile (string, OPTIONAL)
----------------------------------
Name of the `AppArmor <https://wiki.ubuntu.com/AppArmor>`_ profile which will be
applied to the container process by the OCI runtime.
The profile must already be loaded in the kernel and listed under
``/sys/kernel/security/apparmor/profiles``.

selinuxLabel (string, OPTIONAL)
-------------------------------
`SELinux <http://selinuxproject.org/page/Main_Page>`_ label which will be
applied to the container process by the OCI runtime.

selinuxMountLabel (string, OPTIONAL)
------------------------------------
`SELinux <http://selinuxproject.org/page/Main_Page>`_ label which will be
applied to the mounts performed by the OCI runtime into the container.

containersPolicy (object, OPTIONAL)
-----------------------------------
The `containers-policy.json(5) <https://github.com/containers/image/blob/main/docs/containers-policy.json.5.md>`_
file (formally called the "signature verification policy file") is used by
Skopeo and other container tools to define a set of policy requirements
(for example trusted keys) which have to be satisfied in order to qualify a
container image, or individual signatures of that image, as valid and secure
to download.

By default, a user-specific policy is read from ``${HOME}/.config/containers/policy.json``;
if such file does not exists, the system-wide ``/etc/containers/policy.json``
is used instead. This system-wide file is usually provided by the
`containers-common <https://github.com/containers/common>`_ package.

The ``containersPolicy`` object defines fallback and enforcement options for the
policy file and supports the following fields:

* ``path`` (string, REQUIRED): Absolute path to a fallback
  `containers-policy.json(5) <https://github.com/containers/image/blob/main/docs/containers-policy.json.5.md>`_
  file, which will be passed by Sarus to Skopeo in case neither the user-specific
  nor the system-wide default policy files exist. This allows to use a policy
  also on systems which don't have the default files present on all nodes.
  If no default file exists and the ``containersPolicy`` parameter is not defined,
  Sarus throws an error.
* ``enforce`` (bool, OPTIONAL): If true, always use the policy file at
  ``containersPolicy/path``, even if any default file exists. This allows to
  have a Sarus-specific policy different from the one(s) used by other tools
  on the system.

.. important::

   Sarus installations come with a policy file at ``<prefixDir>/etc/policy.json``,
   which is set as the starting value of ``containersPolicy/path``.
   This policy file is very permissive and is in line with the defaults provided
   by package managers for the most popular Linux distributions. It is intended
   only as a starting point in case a system does not feature default policy files.

containersRegistries.dPath (string, OPTIONAL)
---------------------------------------------
Absolute path to a `containers-registries.d(5) <https://github.com/containers/image/blob/main/docs/containers-registries.d.5.md>`_
directory for registries configurations. If defined, this directory will be
used by Skopeo instead of the default ``${HOME}/.config/containers/registries.d``
or ``/etc/containers/registries.d`` directories.


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
        "skopeoPath": "/usr/bin/skopeo",
        "umociPath": "/usr/bin/umoci",
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
        "siteDevices": [
            {
                "source": "/dev/fuse",
                "access": "rw"
            }
        ],
        "environment": {
            "set": {
                "VAR_TO_SET_IN_CONTAINER": "value"
            },
            "prepend": {
                "VAR_WITH_LIST_OF_PATHS_IN_CONTAINER": "/path/to/prepend"
            },
            "append": {
                "VAR_WITH_LIST_OF_PATHS_IN_CONTAINER": "/path/to/append"
            },
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
        "seccompProfile": "/opt/sarus/1.0.0/etc/seccomp/default.json",
        "apparmorProfile": "sarus-default",
        "selinuxLabel": "system_u:system_r:svirt_sarus_t:s0:c124,c675",
        "selinuxMountLabel": "system_u:object_r:svirt_sarus_file_t:s0:c715,c811"
        "containersPolicy": {
            "path": "/opt/sarus/1.0.0/etc/policy.json",
            "enforce": false
        },
        "containersRegistries.dPath": "/opt/sarus/1.0.0/etc/registries.d"
    }
