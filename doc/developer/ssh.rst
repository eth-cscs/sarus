********
SSH Hook
********

Introduction
============

This document is meant to be read by developers in order to understand how Sarus provides SSH within containers.

Overview of the user interface
==============================

First of all, the user needs to run the command ``sarus ssh-keygen``. This command generates the SSH keys
that will be used by the SSH daemons as well as the SSH clients in the containers.

Then the user can execute a container passing the ``--ssh`` option, e.g. ``sarus run --ssh <image> <command>``.
This will first check that the user previously generated the SSH keys, and then will instantiate an SSH daemon
per container and setup a custom ``ssh`` executable inside the containers.

Within a container spawned with the ``--ssh`` option, the user can ssh into other containers by simply issuing the
``ssh`` command available in the default search PATH. E.g.

.. code-block:: bash

    ssh <hostname of other node>

The custom ``ssh`` executable will take care of using the proper keys and non-standard port in order to connect
to the remote container.

Configuration by the system administrator
=========================================

The following is an example of `OCI hook JSON configuration file
<https://github.com/containers/libpod/blob/master/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the SSH hook:

.. literalinclude:: /config/hook_examples/07-ssh-hook.json
   :language: json

Architecture
============

Most of the ssh-related logic is encapsulated in the SSH hook. The SSH hook is an executable binary that
performs different ssh-related operations depending on the CLI command that receives as parameter from the container engine.

The nitty gritty
================

The custom SSH software
-----------------------

The SSH hook uses a custom statically-linked version of `Dropbear <https://matt.ucc.asn.au/dropbear/dropbear.html>`_.
At build time, if the CMake's parameter ``ENABLE_SSH=TRUE`` is specified,
the custom Dropbear is built and installed under the Sarus's installation directory.
Dropbear is built according to the instructions provided in the ``<sarus source dir>/dep/build_dropbear.sh`` script.
Such script also creates a ``localoptions.h`` header, overriding some of the default compile-time options of Dropbear.

How the SSH keys are generated
------------------------------

When the command ``sarus ssh-keygen`` is issued, the command object ``cli::CommandSshkeygen`` gets executed which
in turn executes the SSH hook with the ``keygen`` CLI argument.

The hook performs the following operations:

1. Read from the environment variables the hook base directory.
2. Read from the environment variables the location of the passwd file.
3. Get the username from the passwd file and use it to determine the user's hook directory (where the SSH keys are stored).
4. Read from the environment variables the location of the custom SSH software.
5. Execute the program ``dropbearkey`` to generate two keys in the user's hook directory, using the ECDSA algorithm.
   One key (``dropbear_ecdsa_host_key``) will be used by the SSH daemon,
   the other key (``id_dropbear``) will be used by the SSH client.

How the existence of the SSH keys is checked
--------------------------------------------

When the command ``sarus run --ssh <image> <command>`` is issued, the command object ``cli::CommandRun`` gets
executed which in turn executes the SSH hook with the ``check-user-has-sshkeys`` CLI argument.

The hook performs the following operations:

1. Read from the environment variables the hook base directory.
2. Read from the environment variables the location of the passwd file.
3. Get the username from the passwd file and use it to determine the user's hook directory (where the SSH keys are stored).
4. Check that the user's hook directory contains the SSH keys.

How the SSH daemon and SSH client are setup in the container
------------------------------------------------------------

When the command "sarus run --ssh <image> <command>" is issued, Sarus sets up the OCI bundle and executes
runc. Then runc executes the OCI prestart hooks specified in sarus.json. The system administrator should have
specified the SSH hook with the "start-ssh-daemon" CLI argument.

The hook performs the following operations:

1. Read from the environment variables the hook base directory.
2. Read from the environment variables the location of the passwd file.
3. Read from the environment variables the location of the custom SSH software.
4. Read from the environment variables the port number to be used by the SSH daemon.
5. Read from stdin the container's state as defined in the OCI specification.
6. Enter the container's mount namespaces in order to access the container's OCI bundle.
7. Enter the container's pid namespace in order to start the sshd process inside the container.
8. Read the user's UID/GID from the OCI bundle's config.json
9. Get the username from the passwd file supplied by step 2.
10. Determine the user's hook directory (where the SSH keys are stored) using data from steps 1. and 9.
11. Determine the location of the SSH keys in the container. See :ref:`developer-ssh-keys-in-container`.
12. Copy the custom SSH software into the contaier (SSH daemon, SSH client).
13. Setup the location for SSH keys into the container. If the path from 11.
    does not exist, it is created. If the user mounted their home dir from the
    host, future actions from the hook could alter the host's home dir. For this
    reason, the hook performs an overlay mount over the keys location. This
    allows the hook to copy the Dropbear keys into the container without
    tampering with the user's original ``~/.ssh``.
14. Copy the Dropbear SSH keys into the container.
15. Patch the container's ``/etc/passwd`` if it does not feature a user shell which
    exists inside the container. This check is necessary since with Sarus the
    passwd file is copied from the host system.
16. Create a shell script exporting the environment from the OCI bundle.
    See :ref:`developer-ssh-remote-login-environment`
17. Create a shell script in ``/etc/profile.d``. See :ref:`developer-ssh-remote-login-environment`
18. Chroot to the container, drop privileges, start the SSH deamon inside the container.
19. Create a shell script ``/usr/bin/ssh`` which transparently uses the Dropbear
    SSH client (:program:`dbclient`) and the proper keys and port number to establish SSH sessions.

.. _developer-ssh-keys-in-container:

Determining the location of the SSH keys inside the container
-------------------------------------------------------------

Dropbear looks inside ``~/.ssh``to find the client key, the ``authorized_keys``
and ``known_hosts`` files. The location of the home directory is determined from
the running user's entry in the container's ``/etc/passwd`` file.

The SSH hook reads the container's passwd file as well and extracts the location
of the home directory from that file to ensure the correct operation of Dropbear.
At the moment of copying the SSH keys into the container, the ``~/.ssh`` path is
created if it does not exist. This prevents possible issues with Sarus, which
replaces the ``/etc/passwd`` of the container (see point 3. of :ref:`overview-instantiation-rootfs`):
such file might indicate a user home path which does not initially exist inside the container.

The possibility for a working user to not have a sane home directory inside
``/etc/passwd`` has also to be taken into account: if the home directory field is empty,
for example, the SSH software will still log the user into ``/`` .
Debian-based distributions use ``/nonexistent`` for users which are not supposed to have a home dir.
Red Hat and Alpine assign the root path as home even to the ``nobody`` user.
Dropbear is currently not able to handle these corner cases: if the home field
is empty, Dropbear will fail to find its key even if they are created under ``/.ssh``.
On the other hand, if the passwd file specifies a ``/nonexistent`` home directory field,
Dropbear will still look for ``/nonexistent/.ssh``, which is a location that is
not meant to exist, even if the SSH hook could create it.

Because of the behavior described above, in case of empty or ``/nonexistent`` home directory field
in ``/etc/passwd``, the SSH hook exits with an error.

.. _developer-ssh-remote-login-environment:

Reproducing the remote environment in a login shell
---------------------------------------------------

When opening a login shell into a remote container, the SSH hook also attempts
to reproduce an environment that is as close as possible to the one defined in the
OCI bundle of the remote container.

This feature is implemented through 2 actions which take place when the hook
is executed by the OCI runtime, before the container process is started:

1. A shell script exporting all the environment variables defined in the OCI
bundle is created in ``/opt/oci-hooks/dropbear/environment``.

2. A shell script is created in ``/etc/profile.d/ssh-hook.sh``. This script will
source ``/opt/oci-hooks/dropbear/environment`` (thus replicating the environment from
the OCI bundle) if the ``SSH_CONNECTION`` environment variable is defined:

.. code-block:: bash

   #!/bin/sh
   if [ \"$SSH_CONNECTION\" ]; then
       . /opt/oci-hooks/dropbear/environment
   fi

Dropbear sets the ``SSH_CONNECTION`` variable upon connecting to a remote host.
If the ``ssh`` program is called without a command to execute on the remote,
it will start a login shell. As it is custom, login shells will source ``/etc/profile``,
which in turn will source every script under ``/etc/profile.d``.

Printing debug messages from Dropbear
-------------------------------------

Dropbear gives the possibility to use the ``-v`` command line option on both
the server and client programs (:program:`dropbear` and :program:`dbclient` respectively)
to print debug messages about its internal workings. The ``-v`` option is only
available if the ``DEBUG_TRACE`` symbol is defined as part of the compile-time
options in ``localoptions.h``:

.. code-block:: c

   /* Include verbose debug output, enabled with -v at runtime. */
   #define DEBUG_TRACE 1

The ``<sarus source dir>/dep/build_dropbear.sh`` script defines ``DEBUG_TRACE``,
so in a container launched using the SSH hook it is immediately possible to print
debug messages by passing ``-v`` to the SSH client wrapper script:

.. code-block:: bash

   $ ssh -v <remote container host>  <command>

To obtain debug messages from the Dropbear server, a possibility is to launch
an container with an interactive shell, kill the :program:`dropbear` process
started by the SSH hook and re-launch the server adding the ``-v`` option:

.. code-block::

   $ /opt/oci-hooks/dropbear/bin/dropbear -E -r <user home dir>/.ssh/dropbear_ecdsa_host_key -p <port number from the hook config> -v

Dropbear's server and client programs can print even more detailed messages if
the ``DROPBEAR_TRACE2`` environment variable is defined:

.. code-block:: bash

   $ export DROPBEAR_TRACE2=1
