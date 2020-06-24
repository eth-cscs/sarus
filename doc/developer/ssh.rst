********
SSH Hook
********

Introduction
=============

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

.. code-block:: json

    {
        "version": "1.0.0",
        "hook": {
            "path": "<sarus prefix>/bin/ssh_hook",
            "env": [
                "HOOK_BASE_DIR=/home",
                "PASSWD_FILE=<sarus prefix>/etc/passwd",
                "DROPBEAR_DIR=<sarus prefix>/dropbear",
                "SERVER_PORT=15263"
            ],
            "args": [
                "ssh_hook",
                "start-ssh-daemon"
            ]
        },
        "when": {
            "annotations": {
                "^com.hooks.ssh.enabled$": "^true$"
            }
        },
        "stages": ["prestart"]
    }

Architecture
============

Most of the ssh-related logic is encapsulated in the SSH hook. The SSH hook is an executable binary that
performs different ssh-related operations depending on the CLI command that receives as parameter from Sarus.

The nitty gritty
================

The Sarus's custom SSH software
-------------------------------

Sarus uses a custom statically-linked version of Dropbear. At build time, if the CMake's parameter
ENABLE_SSH=TRUE is specified, the custom Dropbear is built and installed under the Sarus's installation directory.

How the SSH keys are generated
------------------------------

When the command "sarus ssh-keygen" is issued, the command object cli::CommandSshkeygen gets executed which
in turn executes the SSH hook with the "keygen" CLI argument.

The hook performs the following operations:

1. Read from the environment variables the hook base directory.
2. Read from the environment variables the location of the passwd file.
3. Get the username from the passwd file and use it to determine the user's hook directory (where the SSH keys are stored).
4. Read from the environment variables the location of the custom SSH software.
5. Execute the program "dropbearkey" to generate two pairs of public/private keys in the user's hook directory.
   One pair will be used by the SSH daemon, the other pair will be used by the SSH client.

How the existance of the SSH keys is checked
--------------------------------------------

When the command "sarus run --ssh <image> <command>" is issued, the command object cli::CommandRun gets
executed which in turn executes the SSH hook with the "check-user-has-sshkeys" CLI argument.

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
8. Read the container's attributes from the OCI bundle's config.json in order to determine whether
   the SSH hook is enabled.
9. If the SSH hook is disabled exit.
10. Read the user's UID from the OCI bundle's config.json, get the username from the passwd file
    and use it to determine the user's hook directory (where the SSH keys are stored).
11. Copy the custom SSH software into the contaier (SSH daemon, SSH client).
12. Copy the SSH keys into the container.
13. Chroot to the container, drop privileges, start the SSH deamon inside the container.
14. Create a shell script /usr/bin/ssh which transparenty uses the dropbear SSH client (dbclient)
    and the proper keys and port number to establish SSH sessions.
