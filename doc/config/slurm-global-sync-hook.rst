SLURM global sync hook
======================

Sarus also includes the source code for a hook specifically targeting the SLURM
Workload Manager. This hook synchronizes the startup of containers launched
through SLURM, ensuring that all SLURM nodes have spawned a container before
starting the user-requested application in any of them. This kind of
synchronization is useful, for example, when used in conjunction with the
:doc:`SSH hook </config/ssh-hook>`: if the containers are not all available,
some of them might try to establish connections with containers that have yet to
start, causing the whole job step allocation to fail.

When activated, the hook will retrieve information about the current SLURM job
step and node by reading three environment variables: ``SLURM_JOB_ID``,
``SLURM_NTASKS`` and ``SLURM_PROCID``. The hook will then create a job-specific
synchronization directory inside the Sarus local repository of the user. Inside
the synchronization directory, two subdirectories will be created: ``arrival``
and ``departure``. In the ``arrival`` directory each hook from a SLURM node will
create a file with its SLURM process ID as part of the filename, then
periodically check if files from all other SLURM tasks are present in the
directory. The check is performed every 0.1 seconds. When all processes have
signaled their arrival by creating a file, the hooks proceed to signal their
departure by creating a file in the ``departure`` directory, and then exit the
hook program. The hook associated with the SLURM process ID 0 waits for all
other hooks to depart and then cleans up the synchronization directory.

The arrival/departure strategy has been implemented to prevent edge-case racing
conditions: for example, if the file from the last hook is detected and the
cleanup started before the last hook has checked for the presence of all other
files, this situation would result in the deadlock of the last hook.


Hook installation
-----------------

The hook is written in C++ and it will be compiled when building Sarus without
the need of additional dependencies. Sarus's installation scripts will also
automatically install the hook in the ``$CMAKE_INSTALL_PREFIX/bin`` directory. In short,
no specific action is required to install the MPI hook.

Sarus configuration
-------------------

The program is meant to be run as a **prestart** hook and does not accept any
argument. The only required configuration setting is the following environment variable:

* ``SARUS_LOCAL_REPOSITORY_BASE_DIR``: Absolute path to the base directory of
  local user repositories, as configured in the :ref:`corresponding parameter
  <config-reference-localRepositoryBaseDir>` of *sarus.json*.

The following is an example ``OCIHooks`` object enabling the MPI hook:

.. code-block:: json

    {
        "prestart": [
            {
                "path": "/opt/sarus/bin/slurm_global_sync_hook",
                "env": [
                    "SARUS_LOCAL_REPOSITORY_BASE_DIR=<e.g. /home>"
                ]
            }
        ]
    }

Sarus runtime support
---------------------

The SLURM global sync hook is self-sufficient and does not require any support
from the Sarus container runtime.
