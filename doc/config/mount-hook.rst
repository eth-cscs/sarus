**********
Mount hook
**********

The source code of Sarus includes a hook able to perform an arbitrary sequence
of bind mounts and device mounts (including device whitelisting in the related
cgroup) into a container.

When activated, the hook enters the mount namespace of the container and
performs the mounts it received as CLI arguments. The formats for such
arguments are the same as those for the ``--mount`` and ``--device`` options of
``sarus run``. This design choice has several advantages:

* Reuses established formats adopted by popular container tools.

* Explicitly states mount details and provides increased clarity compared to
  lists separated by colons or semicolons (for example, path lists used by other
  hooks).

* Reduces the effort to go from experimentation to consolidation: the mounts
  for a feature can be explored and prototyped on the Sarus command line, then
  transferred directly into a Mount hook configuration file.

In effect, the hook produces the same outcome as entering its ``--mount`` and
``--device`` option arguments in the command line of Sarus
(or other engines with a similar CLI, like Docker and Podman).

However, the hook provides a way to group together the definition and execution
of mounts related to a specific feature.
By doing so, the feature complexity is abstracted from the user and feature
activation becomes either comfortable (e.g. via a single CLI option) or
completely transparent (e.g. if the hook either is always active or if it relies
on an OCI annotation from the container image).
Some example use cases are described in :ref:`this section <mount-hook-config-use-cases>`.

.. note::
   Compared to the :doc:`MPI </config/mpi-hook>` or :doc:`Glibc hooks </config/glibc-hook>`,
   the Mount hook does not check ABI or version compatibility of mounted
   resources, and it does not deduce on its own the mount destination paths
   within the container, since its purpose is not strictly tied to replacing
   library stacks.

Hook installation
=================

The hook is written in C++ and it will be compiled when building Sarus without
the need of additional dependencies. Sarus' installation scripts will also
automatically install the hook in the ``$CMAKE_INSTALL_PREFIX/bin`` directory.
In short, no specific action is required to install the MPI hook.

Hook configuration
==================

The program is meant to be run as a **createContainer** hook and accepts option
arguments with the same formats as the :ref:`--mount <user-custom-mounts>`
or :ref:`--device <user-device-mounts>` options of :program:`sarus run`.

The hook also supports the following environment variables:

* ``LDCONFIG_PATH`` (optional): Absolute path to a trusted ``ldconfig``
  program **on the host**. If set, the program at the path is used
  to update the container's dynamic linker cache after performing the mounts.

The following is an example of `OCI hook JSON configuration file
<https://github.com/containers/common/blob/main/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the MPI hook:

.. literalinclude:: /config/hook_examples/12-mount-hook.json
   :language: json

.. _mount-hook-config-use-cases:

Example use cases
=================

.. _mount-hook-config-use-cases-ofi-provider:

Libfabric provider injection
----------------------------

`Libfabric <https://ofiwg.github.io/libfabric/>`_ is a communication framework
which can be used as a middleware to abstract network hardware from an MPI
implementation. Access to different fabrics is enabled through dedicated
software components, which are called libfabric *providers*.

Fabric provider injection [1]_ consists in bind mounting a dynamically-linked
provider and its dependencies into a container, so that containerized
applications can access a high-performance fabric which is not supported
in the original container image.
For a formal introduction, evaluation, and discussion about the advantages
of this approach, please refer to the reference publication.

.. To implement this approach, the libfabric provider on the host must not be
   built into the libfabric library, but rather be compiled as a separate Dynamic
   Shared Object (DSO). DSO providers are also known as "external" providers
   in libfabric terminology.

.. Compared with the complete MPI replacement performed by the :doc:`MPI </config/mpi-hook>`,
   fabric provider replacement is not limited by MPI implementation compatibility
   and minimizes the modifications to the original image software stack.
   For a formal introduction and detailed evaluation of the fabric provider
   injection approach, please refer to the reference publication.

To facilitate the implementation of fabric provider injection, the Mount hook
supports the ``<FI_PROVIDER_PATH>`` wildcard (angle brackets included)
in ``--mount`` arguments.
``FI_PROVIDER_PATH`` is an environment variable recognized by libfabric itself,
which can be used to control the path where libfabric searches for external,
dynamically-linked providers.
The wildcard is recognized by the hook during the acquisition of CLI arguments,
and is substituted with a path obtained through the following conditions:

* If the ``FI_PROVIDER_PATH`` environment variable exists within the container, its value is taken.

* If ``FI_PROVIDER_PATH`` is unset or empty in the container's environment, and
  the ``LDCONFIG_PATH`` variable is configured for the hook, then the hook searches
  for a libfabric library in the container's dynamic linker cache, and obtains its installation path.
  The wildcard value is then set to ``"libfabric library install path"/libfabric``, which is the default
  search path used by libfabric.
  For example, if libfabric is located at ``/usr/lib64/libfabric.so.1``, the wildcard value
  will be ``/usr/lib64/libfabric``.

* If it's not possible to determine a value with the previous methods, the wildcard value is set to ``/usr/lib``.

The following is an example of hook configuration file using the wildcard to
perform the injection of the GNI provider, enabling access to the Cray Aries
high-speed interconnect on a Cray XC50 supercomputer:

.. literalinclude:: /config/hook_examples/mount-hook-ofi-provider-injection.json
   :language: json

.. According to the ``annotations`` conditions defined, the hook configured in the
   example above could be activated with the ``--mpi-type=libfabric`` option to
   :program:`sarus run`, as described in the :ref:`user guide <user-mpi-hook>`.

.. _mount-hook-config-use-cases-slurm:

Accessing a host Slurm WLM from inside a container
--------------------------------------------------

The Slurm workload manager from the host system can be exposed within containers
through a set of bind mounts. Doing so enables containers to submit new
allocations and jobs to the cluster, opening up the possibility for more
articulated workflows.

The key components to bind mount are the binaries for Slurm commands, the host
Slurm configuration, the MUNGE socket, and any related dependencies.
Below you may find an example of hook configuration file enabling access to the
host Slurm WLM on a Cray XC50 system at CSCS:

.. literalinclude:: /config/hook_examples/mount-hook-slurm.json
   :language: json

The following is an example usage of the hook as configured above:

.. code-block:: bash

   $ srun --pty sarus run --annotation=com.hooks.slurm.activate=true -t debian:11 bash

   nid00040:/$ cat /etc/os-release
   PRETTY_NAME="Debian GNU/Linux 11 (bullseye)"
   NAME="Debian GNU/Linux"
   VERSION_ID="11"
   VERSION="11 (bullseye)"
   VERSION_CODENAME=bullseye
   ID=debian
   HOME_URL="https://www.debian.org/"
   SUPPORT_URL="https://www.debian.org/support"
   BUG_REPORT_URL="https://bugs.debian.org/"

   nid00040:/$ srun --version
   slurm 20.11.8

   nid00040:/$ squeue -u <username>
      JOBID        USER    ACCOUNT   NAME  ST REASON  START_TIME  TIME  TIME_LEFT  NODES  CPUS
     714067  <username>  <account>  sarus   R None      12:48:41  0:40      59:20      1    24

   nid00040:/$ salloc -N4 /bin/bash
   salloc: Waiting for resource configuration
   salloc: Nodes nid0000[0-3] are ready for job

   nid00040:/$ srun -n4 hostname
   nid00002
   nid00003
   nid00000
   nid00001

   # exit from inner Slurm allocation
   nid00040:/$ exit
   # exit from the container
   nid00040:/$ exit

.. rubric:: References

.. [1] A. Madonna and T. Aliaga, "Libfabric-based Injection Solutions for
       Portable Containerized MPI Applications", 2022 IEEE/ACM 4th International
       Workshop on Containers and New Orchestration Paradigms for Isolated
       Environments in HPC (CANOPIE-HPC), Dallas, TX, USA, 2022, pp. 45-56,
       https://doi.org/10.1109/CANOPIE-HPC56864.2022.00010 .
