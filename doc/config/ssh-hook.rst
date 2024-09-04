********
SSH hook
********

Sarus also includes the source code for a hook capable of enabling SSH
connections inside containers. The SSH hook is an executable binary that
performs different ssh-related operations depending on the argument it
receives from the runtime. For the full details about the implementation and
inner workings, please refer to the related :doc:`developer documentation
</developer/ssh>`.

Hook installation
=================

The hook is written in C++ and it will be compiled along with Sarus if the
``ENABLE_SSH=TRUE`` CMake option has been used when configuring the build (the
option is enabled by default). The Sarus installation scripts will also
automatically install the hook in the ``<CMAKE_INSTALL_PREFIX>/bin`` directory.

A custom SSH software (statically linked Dropbear) will also be built and installed in the
``<CMAKE_INSTALL_PREFIX>/dropbear`` directory. This directory must satisfy the
:ref:`security requirements <post-installation-permissions-security>` for critical
files and directories.

Hook configuration
==================

The SSH hook must be configured to run as a **createRuntime** and as a **poststop** hook. 
In the prestart stage the hook sets up the container to accept connections and starts the Dropbear SSH daemon. 
In the poststop stage, cleanup of the SSH daemon process takes place. 
One OCI hook JSON configuration files is sufficient, provided it defines ``"stages": ["prestart", "poststop"]``.

The hook expects to receive its own name/location as the first argument,
and the string ``start-ssh-daemon`` as positional argument. In addition, the following
environment variables must be defined:

* ``HOOK_BASE_DIR``: Absolute base path to the directory where the hook will create and access the SSH keys.
  The keys directory will be located in ``<HOOK_BASE_DIR>/<username>/.oci-hooks/ssh/keys``.

* ``PASSWD_FILE``: Absolute path to a password file (PASSWD(5)).
  The file is used by the hook to retrieve the username of the user.

* ``DROPBEAR_DIR``: Absolute path to the location of the custom SSH software.

* ``SERVER_PORT_DEFAULT``: Default TCP port on which the SSH daemon will listen. This must be an unused
  port and is typically set to a value different than 22 in order to avoid clashes with an OpenSSH
  daemon that could be running on the host. This value can be overridden by setting the
  ``com.hooks.ssh.port`` annotation for the container.

  ``SERVER_PORT_DEFAULT`` takes precedence over the deprecated ``SERVER_PORT``
  environment variable, which serves the same purpose.
  Support for ``SERVER_PORT`` will be removed in a future release.

The following optional environment variables can also be defined:

* ``OVERLAY_MOUNT_HOME_SSH``: When set to ``False`` (case-insensitive), an overlay
  filesystem is not mounted on top of the container's ``${HOME}/.ssh`` directory.
  By default or when this variable is set to ``True`` (case-insensitive), the
  hook creates an overlay filesystem over the container's ``${HOME}/.ssh``
  directory, preventing permanent modifications to its contents.
  For instance, if the user's host ``${HOME}/.ssh`` directory is bind mounted
  within the container, the overlay mount ensures that modifications made by the
  hook (such as adding custom keys and key authorizations) do not propagate back
  to the host.

  It's important to note that full support for rootless overlay is
  available only on Linux kernel versions 5.13 or later. Consequently, if the SSH
  hook is utilized by an unprivileged container runtime, the system's kernel must
  be sufficiently recent to perform the overlay mount.
  Setting ``OVERLAY_MOUNT_HOME_SSH=False`` enables the hook to operate
  successfully on systems with kernels older than 5.13.

  .. warning::

     If ``OVERLAY_MOUNT_HOME_SSH`` is set to ``False`` AND the user's ``${HOME}/.ssh``
     directory is bind mounted from the host into the container, the SSH hook will
     modify the content of the host directory!

     When using If ``OVERLAY_MOUNT_HOME_SSH=False`` it is strongly advised
     to avoid configuring any automatic mount of ``${HOME}`` into the container.
     Additionally, it is very important to clearly communicate to users the
     implications when performing bind mounts related to ``${HOME}``.

* ``JOIN_NAMESPACES``: When set to ``False`` (case-insensitive), the hook does not actively join the mount and PID
  namespaces of the container. This is useful when the hook is executed already inside the appropriate
  namespaces, or when the hook does not have the privileges to join said namespaces.
  By default, the hook always attempts to join the mount and PID namespaces of the container.

The following is an example of `OCI hook JSON configuration file
<https://github.com/containers/common/blob/main/pkg/hooks/docs/oci-hooks.5.md>`_
enabling the SSH hook:

.. literalinclude:: /config/hook_examples/07-ssh-hook.json
   :language: json

The poststop functionality is especially valuable in cases where the hook does not actively join the PID namespace 
of the container. In its absence, the termination of the container would not result in the termination of the 
Dropbear daemon, leading to the persistence of the daemon even after the container has been stopped. 
This persistence can cause issues like port conflicts, as the daemon may still be listening on a port that 
is required by a new container attempting to start.


Sarus support at runtime
========================

The command ``sarus ssh-keygen`` will call the hook without creating a
container, passing the appropriate arguments to generate dedicated keys to be
used by containers.

The ``com.hooks.ssh.enabled=true`` annotation that enables the hook is automatically
generated by Sarus if the ``--ssh`` command line option is passed to :program:`sarus run`.

The ``com.hooks.ssh.pidfile_container`` annotation allows the user to customize the path to the
Dropbear daemon PIDfile inside the container (the default path is ``/opt/oci-hooks/ssh/dropbear/dropbear.pid``).

The ``com.hooks.ssh.pidfile_host`` annotation can be used to copy the PIDfile of the
Dropbear daemon to the specified path on the host.

The ``com.hooks.ssh.port`` annotation can be used to set an arbitrary port for the Dropbear server
and client, overriding the value from the ``SERVER_PORT_DEFAULT`` environment variable set in the hook
configuration file.

.. important::
   The hook currently requires the use of a :ref:`private PID namespace <user-private-pid>`
   for the container. The ``--ssh`` option of :program:`sarus run` implies ``--pid=private``, 
   and is incompatible with the use of ``--pid=host``.
   