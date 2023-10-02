*****************************
Native MPI hook (MPICH-based)
*****************************

Sarus's source code includes a hook able to import native MPICH-based
MPI implementations inside the container. This is useful in case the host system
features a vendor-specific or high-performance MPI stack based on MPICH (e.g.
Intel MPI, Cray MPI, MVAPICH) which is required to fully leverage a high-speed
interconnect.

When activated, the hook will enter the mount namespace of the container, search
for dynamically-linkable MPI libraries and replace them with functional
equivalents bind-mounted from the host system.

In order for the replacements to work seamlessly, the hook will check that the
host and container MPI implementations are ABI-compatible according to the
standards defined by the `MPICH ABI Compatibility Initiative
<https://www.mpich.org/abi/>`_. The Initiative is supported by several
MPICH-based implementations, among which MVAPICH, Intel MPI, and Cray MPT.
ABI compatibility and its implications are further discussed
:doc:`here </user/abi_compatibility>`.

Hook installation
=================

The hook is written in C++ and it will be compiled when building Sarus without
the need of additional dependencies. Sarus' installation scripts will also
automatically install the hook in the ``$CMAKE_INSTALL_PREFIX/bin`` directory.
In short, no specific action is required to install the MPI hook.

Hook configuration
==================

The program is meant to be run as a **prestart** hook and does not accept
arguments, but its actions are controlled through a few environment variables:

* ``LDCONFIG_PATH``: Absolute path to a trusted ``ldconfig``
  program **on the host**.

* ``MPI_LIBS``: Colon separated list of full paths to the host's
  libraries that will substitute the container's libraries. The ABI
  compatibility check is performed by comparing the version numbers specified in
  the libraries' file names as follows:

      - The major numbers (first from the left) must be equal.
      - The host's minor number (second from the left) must be greater or equal
        to the container's minor number. In case the minor number from the
        container is greater than the host's minor number, the hook will print
        a warning but will proceed in the attempt to let the container
        application run.
      - If the host's library name does not contain the version numbers or
        contains only the major version number, the missing numbers are assumed
        to be zero.

  This compatibility check is in agreement with the MPICH ABI version number
  schema.

* ``MPI_DEPENDENCY_LIBS``: Colon separated list of absolute paths to
  libraries that are dependencies of the ``MPI_LIBS``. The hook attempts to
  overlay these libraries onto corresponding libraries within the container.
  However, if the version compatibility check fails due to differences in minor
  versions or the inability to validate compatibility, the libraries are
  mounted to an alternate location to prevent conflicts.
  By default, the path of this directory in the container is ``/opt/mpi_hook``;
  this location is also used for other files and symlinks which the hook may
  need to create in the container.
  By making use of the annotation ``com.hooks.mpi.mount_dir_parent=<libraries parent directory>``,
  the user can customize the parent directory of the designated mount folder for the 
  libraries which don't have a suitable counterpart in the container,
  for example:

  .. code-block:: bash

    $ sarus run --mpi --annotation=com.hooks.mpi.mount_dir_parent=<libraries parent directory> <repo name>/<image name>

  When using the annotation as in the previous example, the hook constructs
  the path for the mounts as ``<libraries parent directory>/mpi_hook/``.

* ``BIND_MOUNTS``: Colon separated list of absolute paths to generic
  files or directories that are required for the correct functionality of the
  host MPI implementation (e.g. specific device files). These resources will
  be bind mounted inside the container with the same path they have on the host.
  If a path corresponds to a device file, that file will be whitelisted for
  read/write access in the container's devices cgroup.

The following is an example of `OCI hook JSON configuration file
<https://github.com/containers/common/blob/main/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the MPI hook:

.. literalinclude:: /config/hook_examples/05-mpi-hook.json
   :language: json

.. _mpi-hook-config-annotations-cli:

Configuring to leverage Sarus annotations and CLI options
=========================================================

Sarus automatically generates the ``com.hooks.mpi.enabled=true`` OCI annotation
if the ``--mpi`` command line option is passed to :program:`sarus run`.
Such annotation can be entered in the hook configuration's ``when`` conditions
to tie the activation of the hook to the presence of the ``--mpi`` option:

.. code-block:: json

   "when": {
       "annotations": {
           "^com.hooks.mpi.enabled$": "^true$"
       }
   }

Additionally, the ``--mpi-type`` option of :program:`sarus run` automatically
generates both the ``com.hooks.mpi.enabled=true`` and ``com.hooks.mpi.type=<value>``
annotations, where the value of ``com.hooks.mpi.type`` is the value passed to
the CLI option.
This allows to configure multiple MPI hooks for different native MPI stacks
(for example differing in implementation, compiler, or underlying communication
framework) and choose one through the :program:`sarus run` command line.
For example:

.. code-block::

   # With these conditions, a hook will be enabled by '--mpi-type=mpich3'
   "when": {
       "annotations": {
           "^com.hooks.mpi.enabled$": "^true$"
           "^com.hooks.mpi.type$": "^mpich3$"
       }
   }

   # With these conditions, a hook will be enabled by '--mpi-type=mpich4-libfabric'
   "when": {
       "annotations": {
           "^com.hooks.mpi.enabled$": "^true$"
           "^com.hooks.mpi.type$": "^mpich4-libfabric$"
       }
   }

When multiple hooks are configured, the :ref:`defaultMPIType <config-reference-defaultMPIType>`
parameter in :doc:`sarus.json </config/basic_configuration>` can be used to
define the default MPI hook for the system and allow users to enable it just
by using the ``--mpi`` option.

.. note::

   The rules for the OCI hook configuration file state that a hook is enabled
   only if *all* the ``when`` conditions match.

   This means that if the administrator did not define ``defaultMPIType`` in the
   Sarus configuration and the user did not provide ``--mpi-type=<value>`` in the CLI
   arguments, the hooks configured with the ``mpi.type`` condition will **NOT** be enabled.

.. note::

   Multiple hooks can be enabled at the same time by configuring them with
   identical conditions.
