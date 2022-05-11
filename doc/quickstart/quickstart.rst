**********
Quickstart
**********

This page provides instructions to quickly deploy Sarus using a standalone
archive containing statically-linked binaries for the engine, the hooks and
some dependency software.

The content in this page is intended for those interested in installing and
using Sarus without building the software beforehand.
If you want to build Sarus from source or through the Spack package manager,
please refer to the :doc:`Custom installation </install/index>` section.


Install Sarus
=============

You can install Sarus by following the steps below.

1. Download the latest standalone Sarus archive from the official `GitHub Releases <https://github.com/eth-cscs/sarus/releases>`_:

   .. code-block:: bash

       mkdir /opt/sarus
       cd /opt/sarus
       # Adjust url to your preferred version
       wget https://github.com/eth-cscs/sarus/releases/download/1.5.0/sarus-Release.tar.gz

2. Extract Sarus in the installation directory:

   .. code-block:: bash

       cd /opt/sarus
       tar xf sarus-Release.tar.gz

3. Run the :ref:`configuration script <configure-installation-script>` to
   finalize the installation of Sarus:

   .. code-block:: bash

       cd /opt/sarus/1.5.0-Release  # adapt folder name to actual version of Sarus
       sudo ./configure_installation.sh

   .. important::
       The configuration script needs to run with root privileges in order to
       set Sarus as a root-owned SUID program.

       The configuration script requires the program ``mksquashfs`` to be installed
       on the system, which is typically available through the ``squashfs-tools`` package.

       As explained by the output of the script, you need to persistently add Sarus to your
       ``PATH``; one option is to add a line like "export PATH=/opt/sarus/bin:${PATH}" to
       your ``.bashrc`` file.

       Also note that the configuration script will create a minimal working configuration.
       For enabling additional features, please refer to the :doc:`/config/configuration_reference`.

4. Perform the :doc:`/install/post-installation`.

.. important::
    The `Skopeo <https://github.com/containers/skopeo>`_ static binary included
    in the Sarus standalone archive is intended for test and evaluation purposes
    only!

    Static builds of Skopeo are `not supported <https://github.com/containers/skopeo/blob/main/install.md#building-a-static-binary>`_
    by Skopeo maintainers, and are unsuitable for production deployments.

    For production uses, it is recommended to :ref:`configure Sarus <config-reference-skopeoPath>`
    to use a dedicated, dynamically-linked Skopeo binary
    (either `installed <https://github.com/containers/skopeo/blob/main/install.md>`_
    through the package manager or built from source).


Use Sarus
=========

Now Sarus is ready to be used. Below is a list of the available commands:

.. code-block:: bash

    help: Print help message about a command
    images: List images
    load: Load the contents of a tarball to create a filesystem image
    pull: Pull an image from a registry
    rmi: Remove an image
    run: Run a command in a new container
    ssh-keygen: Generate the SSH keys in the local repository
    version: Show the Sarus version information

Below is an example of some basic usage of Sarus:

.. code-block:: bash

    $ sarus pull alpine
    # image            : index.docker.io/library/alpine/latest
    # cache directory  : "/home/user/.sarus/cache"
    # temp directory   : "/tmp"
    # images directory : "/home/user/.sarus/images"
    > save image layers ...
    > pulling        : sha256:9d48c3bd43c520dc2784e868a780e976b207cbf493eaff8c6596eb871cbd9609
    > completed      : sha256:9d48c3bd43c520dc2784e868a780e976b207cbf493eaff8c6596eb871cbd9609
    > expanding image layers ...
    > extracting     : "/home/user/.sarus/cache/sha256:9d48c3bd43c520dc2784e868a780e976b207cbf493eaff8c6596eb871cbd9609.tar"
    > make squashfs image: "/home/user/.sarus/images/index.docker.io/library/alpine/latest.squashfs"

    $ sarus images
    REPOSITORY   TAG          IMAGE ID       CREATED               SIZE         SERVER
    alpine       latest       65e50dd72f89   2019-08-21T16:07:06   2.59MB       index.docker.io

    $ sarus run alpine cat /etc/os-release
    NAME="Alpine Linux"
    ID=alpine
    VERSION_ID=3.10.2
    PRETTY_NAME="Alpine Linux v3.10"
    HOME_URL="https://alpinelinux.org/"
    BUG_REPORT_URL="https://bugs.alpinelinux.org/"

.. note::
    You can refer to the section :doc:`User guides </user/index>`
    for more information about how to use Sarus.
