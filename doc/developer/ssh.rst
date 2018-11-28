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
This will first check that the user previously generated the SSH keys, and then will instantiate an sshd
per container and setup a custom ``ssh`` binary inside the containers.

Within a container spawned with the ``--ssh`` option, the user can ssh into other containers by simply issuing the
``ssh`` command available in the default search PATH. E.g.

.. code-block:: bash

    ssh <hostname of other node>

The custom ``ssh`` binary will take care of using the proper keys and non-standard port in order to connect
to the remote container.

Configuration by the system administrator
=========================================

Below is an example of how the system administrator should configure the ``OCIHooks`` entry in the
*sarus.json* in order to enable the SSH hook:

.. code-block:: json

    {
        "OCIHooks": {
            "prestart": [
                {
                    "path": "<sarus prefix>/bin/ssh_hook",
                    "env": [
                        "SARUS_LOCAL_REPOSITORY_BASE_DIR=<e.g. /home>",
                        "SARUS_OPENSSH_DIR=<sarus prefix>/openssh"
                    ],
                    "args": [
                        "ssh_hook",
                        "start-sshd"
                    ]
                }
            ]
        }
    }

Architecture
============

Most of the ssh-related logic is encapsulated in the SSH hook. The SSH hook is an executable binary that
performs different ssh-related operations depending on the CLI command that receives as parameter from Sarus.

The nitty gritty
================

The Sarus's custom OpenSSH
----------------------------

Sarus uses a custom statically-linked version of OpenSSH. At build time, if the CMake's parameter
ENABLE_SSH=TRUE is specified, the custom OpenSSH is built and installed under the Sarus's installation directory.
Two custom configuration files (sshd_config, ssh_config) are also installed along with the various SSH binaries
(ssh, sshd, ssh-keygen, etc.) . The "sshd" and "ssh" custom binaries are configured (hard coded)
to pick the custom configuration files inside the container (sshd_config and ssh_config respectively).
The custom sshd_config file instructs sshd to listen on a specific non-standard port and to only accept
authentications from the SSH key generated through the "sarus ssh-keygen" command. The custom ssh_config
instructs ssh to connect to the non-standard port and to use the SSH key generated through the "sarus ssh-keygen" command.

How the SSH keys are generated
------------------------------

When the command "sarus ssh-keygen" is issued, the command object cli::CommandSshkeygen gets executed which
in turn executes the SSH hook with the "keygen" CLI argument.

The hook performs the following operations:

1. Read from the environment variables the location of the custom OpenSSH as well as the location of the
   user's local repository.
2. Execute the program "ssh-keygen" to generate two pairs of public/private keys. One pair will be used by
   the SSH daemon, the other pair will be used by the SSH client.

How the existance of the SSH keys is checked
--------------------------------------------

When the command "sarus run --ssh <image> <command>" is issued, the command object cli::CommandRun gets
executed which in turn executes the SSH hook with the "check-localrepository-has-sshkeys" CLI argument.

The hook performs the following operations:

1. Read from the environment variables the location of the user's local repository.
2. Check that the user's local repository contains the SSH keys.

How the SSH daemon and SSH client are setup in the container
------------------------------------------------------------

When the command "sarus run --ssh <image> <command>" is issued, Sarus sets up the OCI bundle and executes
runc. Then runc executes the OCI prestart hooks specified in sarus.json. The system administrator should have
specified the SSH hook with the "start-sshd" CLI argument.

The hook performs the following operations:

1. Read from the environment variables the location of the custom OpenSSH as well as the
   location of the user's local repository.
2. Read from stdin the container's state as defined in the OCI specification.
3. Enter the container's mount namespaces in order to access the container's OCI bundle.
4. Enter the container's pid namespace in order to start the sshd process inside the container.
5. Read the container's environment variables from the OCI bundle's config.json in order to determine whether
   the SSH hook is enabled.
6. If the SSH hook is disabled exit.
7. Bind mount the custom OpenSSH (executables + configuration files) into the container.
8. Copy the SSH keys into the container.
9. Add an "sshd" user to /etc/passwd if necessary.
10. Chroot to the container and start sshd inside the container.
11. Bind mount the custom "ssh" binary into the container's /usr/bin, thus the shell
    will pick the custom binary when the command "ssh" is executed.
