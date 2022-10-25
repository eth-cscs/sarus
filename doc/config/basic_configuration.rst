*******************
Basic configuration
*******************

At run time, the configuration parameters of Sarus are read from a file called
*sarus.json*, which is expected to be a valid `JSON document
<https://www.json.org/>`_ located in ``<installation path>/etc``.
Given the :ref:`privileges <post-installation-permissions-execution>` that Sarus
requires in order to exercise its functions, the configuration file must satisfy
specific :ref:`security requirements <post-installation-permissions-security>`
regarding its location and ownership.

.. _configure-installation-script:

Using the ``configure_installation`` script
===========================================

The most straightforward way to generate a basic working configuration for
a Sarus installation is by using the ``configure_installation.sh`` script,
which is found in the top installation directory.

This script will set the proper permissions of the Sarus binary, create the
initial caches for passwd and group databases and fill the necessary parameters
of *sarus.json*.

To accomplish its tasks, the script has to be run as root and has to be located
in the top directory of a valid Sarus installation.

The following is an example script execution (assuming Sarus has been installed
in ``/opt/sarus``) with the corresponding output:

.. code-block:: bash

    $ sudo /opt/sarus/configure_installation.sh
    Setting Sarus as SUID root
    Successfully set Sarus as SUID root
    Creating cached passwd database
    Successfully created cached passwd database
    Creating cached group database
    Successfully created cached group database
    Setting ownership of etc/sarus.schema.json
    Successfully set ownership of etc/sarus.schema.json
    Configuring etc/sarus.json
    Found skopeo: /usr/bin/skopeo
    Found umoci: /usr/bin/umoci
    Found mksquashfs: /usr/bin/mksquashfs
    Found init program: /usr/bin/tini
    Found runc: /usr/local/bin/runc
    Setting local repository base directory to: /home
    Setting centralized repository directory to: /var/sarus/centralized_repository
    Successfully configured etc/sarus.json.
    Configuring etc/hooks.d/07-ssh-hook.json
    Configuring etc/hooks.d/09-slurm-global-sync-hook.json
    To execute sarus commands run first:
    export PATH=/opt/sarus/default/bin:${PATH}
    To persist that for future sessions, consider adding the previous line to your .bashrc or equivalent file

Custom values for the
:ref:`local repositories base directory <config-reference-localRepositoryBaseDir>`
and the
:ref:`centralized image repository <config-reference-centralizedRepositoryDir>`
can be specified by setting the ``SARUS_LOCAL_REPO_BASE_DIR`` and
``SARUS_CENTRALIZED_REPO_DIR`` environment variables, respectively.
For example:

.. code-block:: bash

    $ sudo SARUS_LOCAL_REPO_BASE_DIR=/users SARUS_CENTRALIZED_REPO_DIR=/sarus_centralized_repository /opt/sarus/configure_installation.sh

The configuration script is the recommended way to finalize any installation of
Sarus, regardless of the installation method chosen.

Please note that ``configure_installation.sh`` will only create a baseline
configuration. To enable more advanced features of Sarus, *sarus.json* should
be expanded further. Please refer to the :doc:`/config/configuration_reference`
in order to do so.

The script can also be used on a Sarus installation which has already been
configured. In this case, the existing configuration will be backed up in
``<installation path>/etc/sarus.json.bak`` and a new *sarus.json* file will be
generated.
