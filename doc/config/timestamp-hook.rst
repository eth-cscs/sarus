**************
Timestamp hook
**************

The OCI specifications do not define any requirement on exposing information
about the inner workings of runtimes and hooks to the user. This can make
determining the startup overhead of a standard container runtime difficult.

Sarus bundles a hook which leaves a timestamp on a logfile, accompanied by a
configurable message. Since the OCI Runtime Specification mandates hooks to be
executed in the order they are entered during configuration, interleaving this
hook between other hooks generates useful data to measure the time spent by each
component.

The timestamp has the following format:

``[<UNIX time in seconds.nanoseconds>] [<hostname>-<hook PID>] [hook] [INFO] Timestamp hook: <optional configurable message>``

The following is an actual timestamp example:

``[1552438146.449463] [nid07641-16741] [hook] [INFO] Timestamp hook: After-runc``

This hook does not alter the container in any way, and is primarily meant as a
tool for developers and system administrators working with OCI hooks.


Hook installation
-----------------

The hook is written in C++ and it will be compiled when building Sarus without
the need of additional dependencies. Sarus's installation scripts will also
automatically install the hook in the ``$CMAKE_INSTALL_PREFIX/bin`` directory.
In short, no specific action is required to install the Timestamp hook.

Sarus configuration
-------------------

The Timestamp hook can be configured to run at any of the container lifecycle
phases supported for hook execution (prestart, poststart, poststop), since it is
not tied to the workings of other hooks or the container application.

The hook optionally supports the following environment variable:

* ``TIMESTAMP_HOOK_MESSAGE``: String to display as part of the timestamp printed
  to the target file. This variable is optional, and it is meant to differentiate
  between subsequent invocations of the timestamp hook for the same container.

The following is an example ``OCIHooks`` object enabling the Timestamp hook:

.. code-block:: json

    {
        "prestart": [
            {
                "path": "/opt/sarus/bin/timestamp_hook",
                "env": [
                    "TIMESTAMP_HOOK_MESSAGE=After-runc"
                ]
            }
        ]
    }

As mentioned above, the real value of the Timestamp hook lies in interleaving it
between other hooks in order to have a measurement of the elapsed time.
For example, using the other hooks described in this documentation:

.. code-block:: json

    {
        "prestart": [
            {
                "path": "/opt/sarus/bin/timestamp_hook",
                "env": [
                    "TIMESTAMP_HOOK_MESSAGE=After-runc"
                ]
            },
            {
                "path": "/opt/sarus/bin/nvidia-container-runtime-hook",
                "args": ["nvidia-container-runtime-hook", "-config=/opt/sarus/etc/nvidia-hook-config.toml", "prestart"],
                "env": [
                    "PATH=/usr/local/libnvidia-container_1.0.0-rc.2/bin",
                    "LD_LIBRARY_PATH=/usr/local/libnvidia-container_1.0.0-rc.2/lib"
                ]
            },
            {
                "path": "/opt/sarus/bin/timestamp_hook",
                "env": [
                    "TIMESTAMP_HOOK_MESSAGE=After-NVIDIA-hook"
                ]
            },
            {
                "path": "/opt/sarus/bin/mpi_hook",
                "env": [
                    "SARUS_MPI_LIBS=/usr/lib64/mvapich2-2.2/lib/libmpi.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpicxx.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpifort.so.12.0.5",
                    "SARUS_MPI_DEPENDENCY_LIBS=",
                    "SARUS_MPI_BIND_MOUNTS=",
                    "PATH=/sbin"
                ]
            },
            {
                "path": "/opt/sarus/bin/timestamp_hook",
                "env": [
                    "TIMESTAMP_HOOK_MESSAGE=After-MPI-hook"
                ]
            },
            {
                "path": "/opt/sarus/bin/ssh_hook",
                "env": [
                    "SARUS_LOCAL_REPOSITORY_BASE_DIR=/home",
                    "SARUS_OPENSSH_DIR=/opt/sarus/openssh"
                ],
                "args": [
                    "ssh_hook",
                    "start-sshd"
                ]
            },
            {
                "path": "/opt/sarus/bin/timestamp_hook",
                "env": [
                    "TIMESTAMP_HOOK_MESSAGE=After-SSH-hook"
                ]
            },
            {
                "path": "/opt/sarus/bin/slurm_global_sync_hook",
                "env": [
                    "SARUS_LOCAL_REPOSITORY_BASE_DIR=/home"
                ]
            },
            {
                "path": "/opt/sarus/bin/timestamp_hook",
                "env": [
                    "TIMESTAMP_HOOK_MESSAGE=After-SLURM-sync-hook"
                ]
            }
        ]
    }

The previous example would yield the following output in the logfile:

.. code-block:: bash

    [1552438146.449463] [nid07641-16741] [hook] [INFO] Timestamp hook: After-runc
    [1552438147.334070] [nid07641-16752] [hook] [INFO] Timestamp hook: After-NVIDIA-hook
    [1552438147.463971] [nid07641-16760] [hook] [INFO] Timestamp hook: After-MPI-hook
    [1552438147.502217] [nid07641-16762] [hook] [INFO] Timestamp hook: After-SSH-hook
    [1552438147.624725] [nid07641-16768] [hook] [INFO] Timestamp hook: After-SLURM-sync-hook


Sarus support at runtime
------------------------

The hook is activated by setting the ``TIMESTAMP_HOOK_LOGFILE`` variable in the
*container* environment to the absolute path to the logfile where the hook has
to print its timestamp. Note that the target logfile does not need to exist in
the container's filesystem, since the OCI Runtime Specification mandates hooks
to execute in the runtime namespace.
If the variable is not set, the hook exits without performing any action.

When launching jobs with many containers (e.g. for an MPI application), it is
advisable to point the Timestamp hook to a different file for each container, in
order to avoid filesystem contention and obtain cleaner measurements. The
following example shows one way to achieve this in a batch script for the Slurm
Workload Manager.

.. code-block:: bash

    #!/usr/bin/env bash
    #SBATCH --job-name="sarus"
    #SBATCH --nodes=<NNODES>
    #SBATCH --ntasks-per-node=<NTASKS_PER_NODE>
    #SBATCH --output=job.out
    #SBATCH --time=00:05:00

    echo "SLURM_JOBID="$SLURM_JOBID

    echo "START_TIME=`date +%s`"

    srun bash -c 'file=<JOB_DIR>/out.procid_${SLURM_PROCID}; TIMESTAMP_HOOK_LOGFILE=${file}.timestamp-hook sarus --verbose run --mpi <image> <application> &>${file}.sarus'

    echo "END_TIME=`date +%s`"

The Timestamp hook does not require any direct support from the Sarus container
engine, although it relies on the :ref:`environmental transfer
<user-environmental-transfer>` performed by Sarus to propagate the
``TIMESTAMP_HOOK_LOGFILE`` variable from the host into the container
environment, allowing the hook to work as intended by the user.
