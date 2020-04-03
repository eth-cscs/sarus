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
    Configuring etc/sarus.json
    Successfully configured etc/sarus.json.
    To execute sarus commands run first:
    export PATH=/opt/sarus/bin:${PATH}
    To persist that for future sessions, consider adding the previous line to your .bashrc or equivalent file

The configuration script is the recommended way to finalize any installation of
Sarus, regardless of the installation method chosen.

.. note::

   Usually it is not  necessary to manually run the configuration script after
   installing Sarus through the Spack package manager.
   Unless instructed differently, the Spack package already uses the script
   internally to create a starting configuration.

Please note that ``configure_installation.sh`` will only create a baseline
configuration. To enable more advanced features of Sarus, *sarus.json* should
be expanded further. Please refer to the :doc:`/config/configuration_reference`
in order to do so.

The script can also be used on a Sarus installation which has already been
configured. In this case, the existing configuration will be backed up in
``<installation path>/etc/sarus.json.bak`` and a new *sarus.json* file will be
generated.
