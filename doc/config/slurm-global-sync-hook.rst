Slurm global sync hook
======================

Sarus also includes the source code for a hook specifically targeting the Slurm
Workload Manager. This hook synchronizes the startup of containers launched
through Slurm, ensuring that all Slurm nodes have spawned a container before
starting the user-requested application in any of them. This kind of
synchronization is useful, for example, when used in conjunction with the
:doc:`SSH hook </config/ssh-hook>`: if the containers are not all available,
some of them might try to establish connections with containers that have yet to
start, causing the whole job step execution to fail.

When activated, the hook will retrieve information about the current Slurm job
step and node by reading three environment variables: ``SLURM_JOB_ID``,
``SLURM_NTASKS`` and ``SLURM_PROCID``. The hook will then create a job-specific
synchronization directory inside the Sarus local repository of the user. Inside
the synchronization directory, two subdirectories will be created: ``arrival``
and ``departure``. In the ``arrival`` directory each hook from a Slurm task will
create a file with its Slurm process ID as part of the filename, then
periodically check if files from all other Slurm tasks are present in the
directory. The check is performed every 0.1 seconds. When all processes have
signaled their arrival by creating a file, the hooks proceed to signal their
departure by creating a file in the ``departure`` directory, and then exit the
hook program. The hook associated with the Slurm process ID 0 waits for all
other hooks to depart and then cleans up the synchronization directory.

The arrival/departure strategy has been implemented to prevent edge-case race
conditions: for example, if the file from the last arriving hook is detected and
the cleanup started before the last arriving hook has checked for the presence
of all other files, this situation would result in the deadlock of the last
arriving hook. Having all hooks signal their departure before cleaning up
the synchronization directory avoids this problem.


Hook installation
-----------------

The hook is written in C++ and it will be compiled when building Sarus without
the need of additional dependencies. Sarus's installation scripts will also
automatically install the hook in the ``$CMAKE_INSTALL_PREFIX/bin`` directory.
In short, no specific action is required to install the Slurm global sync hook.

Sarus configuration
-------------------

The program is meant to be run as a **prestart** hook and does not accept any
argument. The only required configuration setting is the following environment variable:

* ``SARUS_PREFIX_DIR``: Absolute path to the installation directory of Sarus.

The following is an example ``OCIHooks`` object enabling the MPI hook:

.. code-block:: json

    {
        "prestart": [
            {
                "path": "/opt/sarus/bin/slurm_global_sync_hook",
                "env": [
                    "SARUS_PREFIX_DIR=/opt/sarus/default"
                ]
            }
        ]
    }

Sarus support at runtime
------------------------

The Slurm global sync hook is self-sufficient and does not require any support
from the Sarus container engine.
