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

The program is meant to be run as a **createContainer** hook and does not accept
arguments, but its actions are controlled through a few environment variables:

* ``LDCONFIG_PATH`` (REQUIRED): Absolute path to a trusted ``ldconfig``
  program **on the host**.

* ``MPI_LIBS`` (REQUIRED): Colon separated list of full paths to the host's
  libraries that will substitute the container's libraries. The ABI
  compatibility is checked by comparing the version numbers specified in
  the libraries' file names according to the specifications selected with the 
  variable ``MPI_COMPATIBILITY_TYPE``.

* ``MPI_COMPATIBILITY_TYPE`` (OPTIONAL): String determining the logic adopted
  to check the ABI compatibility of MPI libraries.
  Must be one of ``major``, ``full``, or ``strict``.
  If not defined, defaults to ``major``.
  The checks performed for compatibility in the different cases are as follows:

  * ``major``

    - The major numbers (first from the left) must be present and equal.

    This is equivalent to checking that the ``soname`` of the libraries are the same.

  * ``full``

    - The major numbers (first from the left) must be present and equal.

    - The host's minor number (second from the left) must be present and greater than
      or equal to the container's minor number. In case the minor number from the
      container is greater than the host's minor number (i.e. the container
      library is probably being replaced with an older revision), the hook
      will print a verbose log message but will proceed in the attempt to let
      the container application run.
    
  * ``strict``
  
    - The major numbers (first from the left) must be present and equal.

    - The host's minor number (second from the left) must be present and equal
      to the container's minor number. 

  This compatibility check is in agreement with the MPICH ABI version number
  schema.

* ``MPI_DEPENDENCY_LIBS`` (OPTIONAL): Colon separated list of absolute paths to
  libraries that are dependencies of the ``MPI_LIBS``. These libraries
  are always bind mounted in the container under ``/usr/lib``.

* ``BIND_MOUNTS`` (OPTIONAL): Colon separated list of absolute paths to generic
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
