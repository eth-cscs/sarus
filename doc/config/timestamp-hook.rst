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

``[1552438146.449463] [nid07641-16741] [hook] [INFO] Timestamp hook: After-runtime``

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

The following is an example of `OCI hook JSON configuration file
<https://github.com/containers/common/blob/main/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the Timestamp hook:

.. literalinclude:: /config/hook_examples/00-timestamp-hook.json
   :language: json

As mentioned above, the real value of the Timestamp hook lies in interleaving it
between other hooks in order to have a measurement of the elapsed time.
For example, using other hooks described in this documentation and creating multiple
Timestamp hook JSON configuration files:

.. code-block:: bash

    $ ls /opt/sarus/etc/hooks.d
    00-timestamp-hook.json
    01-glibc-hook.json
    02-timestamp-hook.json
    03-nvidia-container-toolkit.json
    04-timestamp-hook.json
    05-mpi-hook.json
    06-timestamp-hook.json
    07-ssh-hook.json
    08-timestamp-hook.json
    09-slurm-global-sync-hook.json
    10-timestamp-hook.json

The previous example could produce an output in the logfile like the following:

.. code-block:: bash

    [775589.671527655] [hostname-12385] [hook] [INFO] Timestamp hook: After-runtime
    [775589.675871678] [hostname-12386] [hook] [INFO] Timestamp hook: After-glibc-hook
    [775589.682727735] [hostname-12392] [hook] [INFO] Timestamp hook: After-NVIDIA-hook
    [775589.685961371] [hostname-12393] [hook] [INFO] Timestamp hook: After-MPI-hook
    [775589.690460309] [hostname-12394] [hook] [INFO] Timestamp hook: After-SSH-hook
    [775589.693946863] [hostname-12396] [hook] [INFO] Timestamp hook: After-SLURM-global-sync-hook


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

The Timestamp hook does not require any direct support from the Sarus engine,
but it relies on the presence of the ``TIMESTAMP_HOOK_LOGFILE`` variable in the
container in order to work.
Given the way Sarus creates the :ref:`container environment <user-environment>`,
the variable can be set in two ways:

- in the host environment (like in the example above), having it automatically
  transferred inside the container by Sarus;
- directly in the container by using the ``-e/--env`` option of :program:`sarus run`.
