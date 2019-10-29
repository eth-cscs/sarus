**********
Quickstart
**********


Install Sarus
=============

You can quickly install Sarus by following the simple steps below.

.. important::
    Standalone installation will be available starting from version 1.0.0-rc8.
    For now please refer to :doc:`Custom Installation </install/index>`.

1. Download the latest standalone Sarus archive from the official [GitHub Releases](https://github.com/eth-cscs/sarus/releases).

   .. code-block:: bash

       mkdir /opt/sarus
       cd /opt/sarus
       # Adjust url to your prefered version
       wget https://github.com/eth-cscs/sarus/archive/sarus-1.0.0-rc8.tar.gz

2. Extract Sarus in the installation directory.

   .. code-block:: bash

       cd /opt/sarus
       tar xf sarus.tar.gz

3. Run the :ref:`configuration script <configure-installation-script>` to
   finalize the installation of Sarus.

   .. code-block:: bash

       cd /opt/sarus/1.0.0-rc8 # adapt folder name to actual version of Sarus
       sudo ./configure_installation.sh

   .. important::
       The configuration script needs to run with root privileges in order to
       set Sarus as a root-owned SUID program.

       The configuration script requires the program ``mksquashfs`` to be installed
       on the system, which is typically available through the ``squashfs-tools`` package.

       Also note that the configuration script will create a minimal working configuration.
       For enabling additional features, please refer to the :doc:`/config/configuration_reference`.

   .. note::
       You can refer to the section :doc:`Custom installation </install/index>`
       if you want to build Sarus from source or from the Spack package manager.

   .. important::
      As explained by the output of the previous script, you need to persistently add Sarus to your
      ``PATH``. I.e., something like adding "export PATH=/opt/sarus/bin:${PATH}" to your ``.bashrc``.

4. Perform the :doc:`/install/post-installation`.

   .. note::
      The Sarus binary from the standalone archive looks for SSL certificates
      into the ``/etc/ssl`` directory. Depending on the Linux distribution,
      some certificates may be located in different directories. A possible
      solution to expose the certificates to Sarus is a symlink. For example,
      on CentOS 7::

          sudo ln -s /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem /etc/ssl/cert.pem


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
    REPOSITORY   TAG          DIGEST         CREATED               SIZE         SERVER
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
    for more information on how to use Sarus.
