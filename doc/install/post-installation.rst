*************************
Post-installation actions
*************************

Review permissions required by Sarus
====================================

.. _post-installation-permissions-execution:

During execution
----------------

* Sarus must run as a root-owned SUID executable and be able to achieve full
  root privileges to perform mounts and create namespaces.

* Write/read permissions to the Sarus's centralized repository.
  The system administrator can configure the repository's location through the
  ``centralizedRepositoryDir`` entry in ``sarus.json``.

* Write/read permissions to the users' local image repositories.
  The system administrator can configure the repositories location through the
  ``localRepositoryBaseDir`` entry in ``sarus.json``.

.. _post-installation-permissions-security:

Security related
----------------

Because of the considerable power granted by the requirements above, as a
security measure Sarus will check that critical files and directories opened
during privileged execution meet the following restrictions:

  - They are owned by root.
  - They are writable only by the owner.
  - All their parent directories (up to the root path) are owned by root.
  - All their parent directories (up to the root path) are writable only by the
    owner (no write permissions to group users or other users).

The files checked for the security conditions are:

  - ``sarus.json`` in Sarus's configuration directory ``<sarus install prefix>/etc``.
  - ``sarus.schema.json`` in Sarus's configuration directory ``<sarus install prefix>/etc``.
  - The ``mksquashfs`` utility pointed by ``mksquashfsPath`` in ``sarus.json``.
  - The init binary pointed by ``initPath`` in ``sarus.json``.
  - The OCI-compliant runtime pointed by ``runcPath`` in ``sarus.json``.
  - All the OCI hooks JSON files in the ``hooksDir`` directory specified in ``sarus.json``.
  - All the executables specified in the OCI hooks JSON files.

For directories, the conditions apply recursively for all their contents.
The checked directories are:

  - The directory where Sarus will create the OCI bundle.
    This location can be configured through the ``OCIBundleDir`` entry in
    ``sarus.json``.
  - If the :doc:`SSH Hook </config/ssh-hook>` is installed,
    the directory of the custom OpenSSH software.

Most security checks can be disabled through the :ref:`corresponding parameter
<config-reference-securityChecks>` in the Sarus configuration file.
Checks on ``sarus.json`` and ``sarus.schema.json`` will always be performed,
regardless of the parameter value.

.. important::

    The ability to disable security checks is only meant as a convenience
    feature when rapidly iterating over test and development installations.
    It is strongly recommended to keep the checks enabled for production
    deployments.


Load required kernel modules
============================

If the kernel modules listed in :doc:`requirements` are not loaded automatically
by the system, remember to load them manually:

.. code-block:: bash

    sudo modprobe loop
    sudo modprobe squashfs
    sudo modprobe overlay


Automatic update of Sarus' passwd cache
=======================================

When executing the :ref:`configure_installation script <configure-installation-script>`,
the passwd and group information are copied and cached
into ``<sarus install prefix>/etc/passwd`` and ``<sarus install prefix>/etc/group``
respectively. The cache allows to bypass the host's passwd/group database, e.g.
LDAP, which could be tricky to configure and access from the container. However,
since the cache is created/updated only once at installation time, it can
quickly get out-of-sync with the actual passwd/group information of the system.
A possible solution is to periodically run a cron job to refresh the
cache. E.g. a cron job and a script like the ones below would do:

.. code-block:: bash

    $ crontab -l
    5 0 * * * update_sarus_user.sh

.. code-block:: bash

    $ cat update_sarus_user.sh

    #!/bin/bash

    /usr/bin/getent passwd > <sarus install prefix>/etc/passwd
    /usr/bin/getent group  > <sarus install prefix>/etc/group
