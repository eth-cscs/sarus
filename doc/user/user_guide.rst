**********
User guide
**********

Basic end-to-end user workflow
==============================

Steps to do **on your workstation**:

1. Build the container image using the Docker tool and Dockerfiles.

2. Push the Docker image into Docker Hub registry (https://hub.docker.com).

Steps to do **on the HPC system**:

3. Pull the Docker image from the Docker Hub registry using Sarus.

4. Run the image at scale with Sarus.

An explanation of the different steps required to deploy a Docker image using
Sarus follows.

1. Develop the Docker image
---------------------------

First, the user has to build a container image. This boils down to
writing a Dockerfile that describes the container, executing the Docker
command line tool to build the image, and then running the container to
test it. Below you can find an overview of what the development process
looks like. We provide a brief introduction to using Docker, however,
for a detailed explanation please refer to the `Docker Get Started
guide <https://docs.docker.com/engine/getstarted/>`_.

Let's start with a simple example. Consider the following Dockerfile
where we install Python on a Debian Jessie base image::

    FROM debian:jessie
    RUN apt-get -y update && apt-get install -y python

Once that the user has written the Dockerfile, the container
image can be built:

.. code-block:: bash

    $ docker build -t hello-python .

The previous step will take the content of the Dockerfile and build our
container image. The first entry, ``FROM debian:jessie``, will specify a base
Linux distribution image as a starting point for our container (in this case,
Debian Jessie), the image of which Docker will try to fetch from its registry if
it is not already locally available. The second entry ``RUN apt-get -y update &&
apt-get install -y python`` will execute the command that follows the ``RUN``
instruction, updating the container with the changes done to its software
environment: in this case, we are installing Python using the apt-get package
manager.

Once the build step has finished, we can list the available container
images using:

.. code-block:: bash

    $ docker images
    REPOSITORY        TAG        IMAGE ID         CREATED           SIZE
    hello-python      latest     2e57316387c6     3 minutes ago     224 MB

We can now spawn a container from the image we just built (tagged as
``hello-python``), and run Python inside the container
(``python --version``), so as to verify the version of the installed
interpreter:

.. code-block:: bash

    $ docker run --rm hello-python python --version
    Python 2.7.9

One of the conveniences of building containers with Docker is that this
process can be carried out solely on the user's workstation/laptop,
enabling quick iterations of building, modifying, and testing of
containerized applications that can then be easily deployed on a variety
of systems, greatly improving user productivity.

.. note::

    Reducing the size of the container image, besides saving disk space,
    also speeds up the process of importing it into Sarus later on.
    The easiest ways to limit image size are cleaning the package manager cache
    after installations and deleting source codes and other intermediate build
    artifacts when building software manually.
    For practical examples and general good advice on writing Dockerfiles,
    please refer to the official `Best practices for writing Dockerfiles
    <https://docs.docker.com/engine/userguide/eng-image/dockerfile_best-practices/>`_.

A user can also access the container interactively through a shell,
enabling quick testing of commands. This can be a useful step for
evaluating commands before adding them to the Dockerfile:

.. code-block:: bash

    $ docker run --rm -it hello-python bash
    root@c5fc1954b19d:/# python --version
    Python 2.7.9
    root@c5fc1954b19d:/#

2. Push the Docker image to the Docker Hub
------------------------------------------

Once the image has been built and tested, you can log in to `DockerHub <https://hub.docker.com/>`_
(requires an account) and push it, so that it becomes available from the cloud:

.. code-block:: bash

    $ docker login
    $ docker push <user name>/<repo name>:<image tag>

Note that in order for the push to succeed, the image has to be
correctly tagged with the same ``<repository name>/<image name>:<image tag>``
identifier you intend to push to. Images can be tagged at build-time, supplying
the ``-t`` option to :program:`docker build`, or afterwards by using
:program:`docker tag`. In the case of our example:

.. code-block:: bash

    $ docker tag hello-python <repo name>/hello-python:1.0


The image tag (the last part of the identifier after the colon) is optional.
If absent, Docker will set the tag to ``latest`` by default.

3. Pull the Docker image from Docker Hub
----------------------------------------

Now the image is stored in the Docker Hub and you can pull it into the HPC
system using the :program:`sarus pull` command followed by the image
identifier:

.. code-block:: bash

    $ sarus pull <repo name>/hello-python:1.0

While performing the pull does not require specific privileges, it is generally
advisable to run :program:`sarus pull` on the system's compute nodes through
the workload manager: compute nodes often have better hardware and, in some
cases like Cray XC systems, large RAM filesystems, which will greatly reduce
the pull process time and will allow to pull larger images.

Should you run into problems because the pulled image doesn't fit in the default
filesystem, you can specify an alternative temporary directory with the
``--temp-dir`` option.

You can use :program:`sarus images` to list the images available on the system:

.. code-block:: bash

    $ sarus images
    REPOSITORY                 TAG       IMAGE ID       CREATED               SIZE         SERVER
    <repo name>/hello-python   1.0       6bc9d2cd1831   2018-01-19T09:43:04   40.16MB      docker.io

4. Run the image at scale with Sarus
------------------------------------

Once the image is available to Sarus we can run it at scale using the workload
manager. For example, if using SLURM:

.. code-block:: bash

    $ srun -N 1 sarus run <repo name>/hello-python:1.0 python --version
    Python 2.7.9

As with Docker, containers can also be used through a terminal,
enabling quick testing of commands:

.. code-block:: bash

    $ srun -N 1 --pty sarus run -t debian bash
    $ cat /etc/os-release
    PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
    NAME="Debian GNU/Linux"
    VERSION_ID="9"
    VERSION="9 (stretch)"
    ID=debian
    HOME_URL="https://www.debian.org/"
    SUPPORT_URL="https://www.debian.org/support"
    BUG_REPORT_URL="https://bugs.debian.org/"

    $ exit

The ``--pty`` option to ``srun`` and the ``-t/--tty`` option to ``sarus run``
are needed to properly setup the pseudo-terminals in order to achieve a familiar
user experience.

You can tell the previous example was run inside a container by querying the
specifications of your host system. For example, the OS of Cray XC compute nodes
is based on SLES and not Debian:

.. code-block:: bash

    $ srun -N 1 cat /etc/os-release
    NAME="SLES"
    VERSION="12"
    VERSION_ID="12"
    PRETTY_NAME="SUSE Linux Enterprise Server 12"
    ID="sles"
    ANSI_COLOR="0;32"
    CPE_NAME="cpe:/o:suse:sles:12"


Additional features
===================

Pulling images from 3rd party registries
----------------------------------------

By default, Sarus tries to pull images from `Docker Hub <https://hub.docker.com/>`_.
To pull an image from a registry different from Docker Hub, enter the server
address as part of the image reference. For example, to access the NVIDIA GPU
Cloud:

.. code-block:: bash

    $ srun -N 1 sarus pull --login nvcr.io/nvidia/k8s/cuda-sample:nbody
    username    :$oauthtoken
    password    :
    ...

To work with images not pulled from Docker Hub (including the removal detailed
in a later section), you need to enter the image reference
as displayed by the :program:`sarus images` command in the first two columns:

.. code-block:: bash

    $ sarus images
    REPOSITORY                       TAG          IMAGE ID       CREATED               SIZE         SERVER
    nvcr.io/nvidia/k8s/cuda-sample   nbody        29e2298d9f71   2019-01-14T12:22:25   91.88MB      nvcr.io

    $ srun -N1 sarus run nvcr.io/nvidia/k8s/cuda-sample:nbody cat /usr/local/cuda/version.txt
    CUDA Version 9.0.176

Pulling images from private repositories
----------------------------------------

Remote registries may host image repositories which are not public, but require
credentials in order to access their content. To retrieve images from a private
repository, the :program:`sarus pull` command offers several options.

The ``--login`` option allows to enter credentials through an interactive
prompt after launching the command:

.. code-block:: bash

    $ srun -N 1 sarus pull --login <privateRepo>/<image>:<tag>
    username: user
    password:
    ...

``--login`` also supports piping to stdin, at the condition that user name and
password are separated by a newline character:

.. code-block:: bash

    $ srun -N 1 printf '<user>\n<password>' | sarus pull --login <privateRepo>/<image>:<tag>

The ``-u/--user`` option allows to provide the user name as part of the command
line, while the ``--password-stdin`` option reads the password from stdin. These
two options complement each other naturally:

.. code-block:: bash

    $ srun -N 1 cat passwordFile.txt | sarus pull --username <user> --password-stdin <privateRepo>/<image>:<tag>

It is also possible to combine the ``-u/--user`` option with ``--login`` (for
example to automatically populate the username field in the interactive prompt);
on the other hand, the ``--password-stdin`` option cannot be used in conjunction
with ``--login``.

Managing the registry authentication file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sarus internally relies on `Skopeo <https://github.com/containers/skopeo>`_ to
download images from remote registries. When using the ``--login`` option, Sarus
passes credentials to Skopeo through a
`containers-auth.json(5) <https://github.com/containers/image/blob/main/docs/containers-auth.json.5.md>`_
authentication file, which is generated using data only for the specific
repository being accessed.

The authentication file is created within the ``${XDG_RUNTIME_DIR}/sarus``
directory if the ``XDG_RUNTIME_DIR`` environment variable is defined and its
value is the path to an existing directory.
Otherwise, the file is created in the Sarus local repository for the current user.

.. note::
    The path to the local repository for a given user can be obtained from the
    :ref:`localRepositoryBaseDir <config-reference-localRepositoryBaseDir>` parameter
    in the Sarus :doc:`configuration file </config/basic_configuration>`
    according to the pattern ``<localRepositoryBaseDir>/<user name>/.sarus``.

The file is owned by the user who launched the :program:`sarus pull` command, and
is set to have owner-only read and write access to prevent exposing the registry
credentials.

The authentication file generated by Sarus is intended to be specific for each
pull invocation, and is automatically removed at the end of a successful pull
process.
If an image pull terminates abnormally, the file may be left lingering in the
filesystem. In such case, it can be removed manually by the owner, or it will
be overwritten and removed by the next successful :program:`sarus pull` of a
private image.

Pulling images by digest (immutable identifier)
-----------------------------------------------

Container images are usually pulled using a *tag*, which is an arbitrary label
to differentiate images within the same repository.
Image tags are mutable, and can potentially point to different images at
different times, for example in the case an image is rebuilt.

Sarus supports the capability to pull images using a *digest*, an immutable
identifier which uniquely and consistently points to a specific version of an
image.
Digests are useful to increase clarity and reproducibility in container workflows
by allowing to reference exact software stack versions.

As `defined <https://github.com/opencontainers/image-spec/blob/main/descriptor.md#digests>`_
by the OCI Image Specification, digests take the form of a string using the
``<algorithm>:<encoded>`` pattern. The ``algorithm`` portion indicates the
cryptographic algorithm used for the digest, while the ``encoded`` portion
represents the result of the hash function.

To pull an image by digest, append the digest to the image name using ``@`` as
separator:

.. code-block:: bash

    $ sarus pull debian@sha256:039f72a400b48c272c6348f0a3f749509b18e611901a21379abc7eb6edd53392
    # image            : docker.io/library/debian@sha256:039f72a400b48c272c6348f0a3f749509b18e611901a21379abc7eb6edd53392
    # cache directory  : "/home/<user>/.sarus/cache"
    # temp directory   : "/tmp"
    # images directory : "/home/<user>/.sarus/images"
    # image digest     : sha256:039f72a400b48c272c6348f0a3f749509b18e611901a21379abc7eb6edd53392
    Getting image source signatures
    Copying blob 5492f66d2700 done
    Copying config 3c3ca0ede6 done
    Writing manifest to image destination
    Storing signatures
    > unpacking OCI image
    > make squashfs image: "/home/<user>/.sarus/images/docker.io/library/debian/sha256-039f72a400b48c272c6348f0a3f749509b18e611901a21379abc7eb6edd53392.squashfs"

It is possible to combine tag and digest in the argument of the :program:`sarus pull`
command. In this case, Sarus proceeds to pull the image indicated by the digest
and completely ignores the tag. This behavior is consistent with other container
tools like Docker, Podman and Buildah:

.. code-block::

    $ sarus pull alpine:3.15.2@sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97
    # image            : docker.io/library/alpine:3.15.2@sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97
    # cache directory  : "/home/<user>/.sarus/cache"
    # temp directory   : "/tmp"
    # images directory : "/home/<user>/.sarus/images"
    # image digest     : sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97
    Getting image source signatures
    Copying blob 3aa4d0bbde19 done
    Copying config e367198082 done
    Writing manifest to image destination
    Storing signatures
    > unpacking OCI image
    > make squashfs image: "/home/<user>/.sarus/images/docker.io/library/alpine/sha256-73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97.squashfs"

Even if the tag is ignored by the pull, it can still serve as a visual aid for
for users writing or reading the command, helping to understand what image the
digest is pointing to.

Pulling images using a proxy
----------------------------

Sarus delegates communication with remote registries to `Skopeo
<https://github.com/containers/skopeo>`_, which in turn uses the Golang standard
library to implement http connections. The usage of a proxy by the Golang
`http package <https://pkg.go.dev/net/http>`_ can be controlled through a set of
`environment variables <https://pkg.go.dev/net/http#ProxyFromEnvironment>`_.

The ``HTTP_PROXY`` and ``HTTPS_PROXY`` variables (or their lowercase versions)
are used to indicate proxies for the respective protocols. For https requests,
``HTTPS_PROXY`` takes precedence over ``HTTP_PROXY``.

To connect to a specific registry without going through the proxy, the
``NO_PROXY`` or ``no_proxy`` environment variables can be used. Such variables
should contain a list of comma-separated values for which a proxy connection is
not used.

The values of the environment variables may be either a complete URL or in a
``host[:port]`` format, in which case the ``http`` scheme is assumed.
The supported schemes are ``http``, ``https``, and ``socks5``.
For example:

.. code-block:: bash

    http_proxy=socket5://127.0.0.1:3128
    https_proxy=https://127.0.0.1:3128
    no_proxy=localhost,127.0.0.1,*.docker.io

Pulling images from insecure registries
---------------------------------------

Images from remote registries are downloaded through
`Skopeo <https://github.com/containers/skopeo>`_, which by default uses secure
connections and the TLS protocol.

To enable image pulls from a registry unable to provide the required TLS
certificates, the registry must be declared as "insecure" in a `registry
configuration file <https://github.com/containers/image/blob/main/docs/containers-registries.conf.5.md>`_
for container tools.

The main registry configuration file for a given user is located in
``${HOME}/.config/containers/registries.conf``; if such file does not exist,
``/etc/containers/registries.conf`` is used instead.

In addition to the main file, specific drop-in configuration files can be
created in ``${HOME}/.config/containers/registries.conf.d`` or in
``/etc/containers/registries.conf.d``. Drop-in configuration files are loaded
after the main configuration, and their values will overwrite any previous setting.
For more details about drop-in configuration directories, please refer to the
`containers-registries.conf.d(5) <https://github.com/containers/image/blob/main/docs/containers-registries.conf.d.5.md>`_
manpage.

The following example shows how to create a drop-in registry configuration file
defining an insecure registry located at ``localhost:5000``:

.. code-block:: bash

    $ mkdir -p ${HOME}/.config/containers/registries.conf.d
    $ cat <<EOF > ${HOME}/.config/containers/registries.conf.d/insecure-localhost.conf
    > [[registry]]
    > prefix = "localhost:5000"
    > insecure = true
    > location = "localhost:5000"
    > EOF

Notice the ``insecure = true`` setting. For the full details about the format
and the available settings of a container registry configuration file, please
refer to the `containers-registries.conf(5)
<https://github.com/containers/image/blob/main/docs/containers-registries.conf.5.md>`_
manpage.

Once an insecure registry has been entered in a configuration file, it is
possible to pull images from it:

.. code-block:: bash

    $ sarus pull localhost:5000/library/alpine
    # image            : localhost:5000/library/alpine:latest
    # cache directory  : "/home/docker/.sarus/cache"
    # temp directory   : "/tmp"
    # images directory : "/home/docker/.sarus/images"
    # image digest     : sha256:e7d88de73db3d3fd9b2d63aa7f447a10fd0220b7cbf39803c803f2af9ba256b3
    Getting image source signatures
    Copying blob 59bf1c3509f3 done
    Copying config d539cd357a done
    Writing manifest to image destination
    Storing signatures
    > unpacking OCI image
    > make squashfs image: "/home/docker/.sarus/images/localhost:5000/library/alpine/latest.squashfs"

    $ sarus images
    REPOSITORY                      TAG          IMAGE ID       CREATED               SIZE         SERVER
    localhost:5000/library/alpine   latest       d539cd357acb   2022-03-15T17:24:08   2.61MB       localhost:5000

.. warning::

    Please note that the use of insecure registries represents a significant
    security risk, and as such should only be used in exceptional cases such as
    local testing.

Download cache
--------------

During image pulls, Sarus stores individual image components (like filesystem
layers and OCI configuration files) downloaded from registries in a cache
directory, so they can be reused by subsequent pull commands.

The location of the cache directory for the current user is displayed at the
beginning of the :program:`sarus pull` command output, alongside image properties
and other directories used by Sarus.
The location of the cache can also be obtained from the
:ref:`localRepositoryBaseDir <config-reference-localRepositoryBaseDir>` parameter
in the ``sarus.json`` :doc:`configuration file </config/basic_configuration>`,
using the following path format ``<localRepositoryBaseDir>/<username>/.sarus/cache``.

The contents of the download cache can be deleted at any time to free up storage
space.

.. _user-load-archive:

Loading images from tar archives
--------------------------------

If you do not have access to a remote Docker registry, or you're uncomfortable
with uploading your images to the cloud in order to pull them, Sarus offers
the possibility to load images from tar archives generated by
`docker save <https://docs.docker.com/engine/reference/commandline/save/>`_.

First, save an image to a tar archive using Docker *on your workstation*:

.. code-block:: bash

    $ docker save --output debian.tar debian:jessie
    $ ls -sh
    total 124M
    124M debian.tar

Then, transfer the archive to the HPC system and use the :program:`sarus load`
command, followed by the archive filename and the reference you want to give to
the Sarus image:

.. code-block:: bash

    $ sarus images
    REPOSITORY       TAG          IMAGE ID       CREATED      SIZE         SERVER

    $ srun sarus load ./debian.tar my_debian
    > expand image layers ...
    > extracting     : /tmp/debian.tar/7e5c6402903b327fc62d1144f247c91c8e85c6f7b64903b8be289828285d502e/layer.tar
    > make squashfs ...
    > create metadata ...
    # created: <user home>/.sarus/images/load/library/my_debian/latest.squashfs
    # created: <user home>/.sarus/images/load/library/my_debian/latest.meta

    $ sarus images
    REPOSITORY               TAG          IMAGE ID       CREATED               SIZE         SERVER
    load/library/my_debian   latest       2fe79f06fa6d   2018-01-31T15:08:56   47.04MB      load

The image is now ready to use. Notice that the origin server for the image has
been labeled ``load`` to indicate this image has been loaded from an archive.

Similarly to :program:`sarus pull`, we recommend to load tar archives from
compute nodes. Should you run out of space while unpacking the image,
:program:`sarus load` also accepts the ``--temp-dir`` option to specify an
alternative unpacking directory.

As with images from 3rd party registries, to use or remove loaded images you
need to enter the image reference as displayed by the :program:`sarus images`
command in the first two columns (repository[:tag]).

Displaying image digests
------------------------

Images pulled by digest do not have a tag associated to them. In order to run
or remove such images, it is necessary to provide the full digest after the
image name. The digests of the images available in the Sarus local repository
can be displayed using the ``--digests`` option of the :program:`sarus images`
command:

.. code-block::

    $ sarus images --digests
    REPOSITORY   TAG          DIGEST                                                                    IMAGE ID       CREATED               SIZE         SERVER
    alpine       latest       sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97   e3671980822d   2022-03-25T13:17:13   2.61MB       docker.io
    fedora       latest       sha256:36af84ba69e21c9ef86a0424a090674c433b2b80c2462e57503886f1d823abe8   04d13a5c8de5   2022-03-25T13:17:57   50.03MB      docker.io
    ubuntu       <none>       sha256:dcc176d1ab45d154b767be03c703a35fe0df16cfb1cc7ea5dd3b6f9af99b6718   4f4768f23ea4   2022-03-25T13:21:40   26.41MB      docker.io

Running images by digest
------------------------

To run images pulled by digest, append the digest to the image name using
``@`` as separator:

.. code-block::

    $ sarus images --digests
    REPOSITORY   TAG          DIGEST                                                                    IMAGE ID       CREATED               SIZE         SERVER
    alpine       <none>       sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97   e3671980822d   2022-03-25T14:28:45   2.61MB       docker.io

    $ sarus run alpine@sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97 cat /etc/os-release
    NAME="Alpine Linux"
    ID=alpine
    VERSION_ID=3.15.2
    PRETTY_NAME="Alpine Linux v3.15"
    HOME_URL="https://alpinelinux.org/"
    BUG_REPORT_URL="https://bugs.alpinelinux.org/"

As with the :program:`sarus pull` command, if both tag and digest are specified
the tag is ignored and the image is looked up using the digest:

.. code-block::

    $ sarus run alpine:1.0@sha256:73c155696fe65b68696e6ea24088693546ac468b3e14542f23f0efbde289cc97 cat /etc/os-release
    NAME="Alpine Linux"
    ID=alpine
    VERSION_ID=3.15.2
    PRETTY_NAME="Alpine Linux v3.15"
    HOME_URL="https://alpinelinux.org/"
    BUG_REPORT_URL="https://bugs.alpinelinux.org/"

Removing images
---------------

To remove an image from Sarus's local repository, use the :program:`sarus
rmi` command:

.. code-block:: bash

    $ sarus images
    REPOSITORY       TAG          IMAGE ID       CREATED               SIZE         SERVER
    library/debian   latest       6bc9d2cd1831   2018-01-31T14:11:27   40.17MB      docker.io

    $ sarus rmi debian:latest
    removed docker.io/library/debian/latest

    $ sarus images
    REPOSITORY   TAG          IMAGE ID       CREATED      SIZE         SERVER

To remove images pulled from 3rd party registries or images loaded from local
tar archives you need to enter the image reference as
displayed by the :program:`sarus images` command:

.. code-block:: bash

    $ sarus images
    REPOSITORY               TAG          IMAGE ID       CREATED               SIZE         SERVER
    load/library/my_debian   latest       2fe79f06fa6d   2018-01-31T15:08:56   47.04MB      load

    $ sarus rmi load/library/my_debian
    removed load/library/my_debian/latest

To remove images pulled by digest, append the digest to the image name using
``@`` as separator:

.. code-block::

    $ sarus rmi ubuntu@sha256:dcc176d1ab45d154b767be03c703a35fe0df16cfb1cc7ea5dd3b6f9af99b6718
    removed image docker.io/library/ubuntu@sha256:dcc176d1ab45d154b767be03c703a35fe0df16cfb1cc7ea5dd3b6f9af99b6718

.. _user-environment:

Environment
-----------

Environment variables within containers are set by combining several sources,
in the following order (later entries override earlier entries):

1. Host environment of the process calling Sarus
2. Environment variables defined in the container image, e.g., Docker ENV-defined variables
3. Modification of variables related to the :ref:`NVIDIA Container Toolkit
   <config-hooks-nvidia-support>`
4. Modifications (set/prepend/append/unset) specified by the system administrator
   in the Sarus configuration file.
   See :ref:`here <config-reference-environment>` for details.
5. Environment variables defined using the ``-e/--env`` option of :program:`sarus run`.
   The option can be passed multiple times, defining one variable per option.
   The first occurring ``=`` (equals sign) character in the option value is
   treated as the separator between the variable name and its value:

   .. code-block:: bash

       $ srun sarus run -e SARUS_CONTAINER=true debian bash -c 'echo $SARUS_CONTAINER'
       SARUS_CONTAINER=true

       $ srun sarus run --env=CLI_VAR=cli_value debian bash -c 'echo $CLI_VAR'
       CLI_VAR=cli_value

       $ srun sarus run --env NESTED=innerName=innerValue debian bash -c 'echo $NESTED'
       NESTED=innerName=innerValue

   If an ``=`` is not provided in the option value, Sarus considers the string
   as the variable name, and takes the value from the corresponding variable
   in the host environment. This can be used to override a variable set in the
   image with the value from the host.
   If no ``=`` is provided and a matching variable is not found in the host
   environment, the option is ignored and the variable is not set in the container.


Accessing host directories from the container
---------------------------------------------

System administrators can configure Sarus to automatically mount facilities
like parallel filesystems into every container. Refer to your site documentation
or system administrator to know which resources have been enabled on a specific
system.

.. _user-custom-mounts:

Mounting custom files and directories into the container
--------------------------------------------------------

By default, Sarus creates the container filesystem environment from the image
and host system as specified by the system administrator.
It is possible to request additional paths from the host environment to be
mapped to some other path within the container using the ``--mount`` option of
the :program:`sarus run` command:

.. code-block:: bash

    $ srun -N 1 --pty sarus run --mount=type=bind,source=/path/to/my/data,destination=/data -t debian bash

The previous command would cause ``/path/to/my/data`` on the host to be mounted
as ``/data`` within the container. This mount option can be specified multiple
times, one for each mount to be performed.
``--mount`` accepts a comma-separated list of ``<key>=<value>`` pairs as its
argument, much alike the Docker option with the same name (for reference, see
the `official Docker documentation on bind mounts
<https://docs.docker.com/engine/admin/volumes/bind-mounts/>`_). As with Docker,
the order of the keys is not significant. A detailed breakdown of the possible
flags follows in the next subsections.

Mandatory flags
^^^^^^^^^^^^^^^

* ``type``: represents the type of the mount. Currently, only ``bind``
  (for bind-mounts) is supported.
* ``source`` (required): Absolute path accessible from the user *on the host*
  that will be mounted in the container. Can alternatively be specified as ``src``.
* ``destination``: Absolute path to where the filesystem will be made available
  inside the container. If the directory does not exist, it will be created.
  It is possible to overwrite other bind mounts already present in the
  container, however, the system administrator retains the power to disallow
  user-requested mounts to any location at their discretion.
  May alternatively be specified as ``dst`` or ``target``.

Bind mounts
^^^^^^^^^^^
In addition to the mandatory flags, regular bind mounts can optionally
add the following flag:

* ``readonly`` (optional): Causes the filesystem to be mounted as read-only.
  This flag takes no value.

The following example demonstrates the use of a custom read-only bind mount.

.. code-block:: bash

    $ ls -l /input_data
    drwxr-xr-x.  2 root root      57 Feb  7 10:49 ./
    drwxr-xr-x. 23 root root    4096 Feb  7 10:49 ../
    -rw-r--r--.  1 root root 1048576 Feb  7 10:49 data1.csv
    -rw-r--r--.  1 root root 1048576 Feb  7 10:49 data2.csv
    -rw-r--r--.  1 root root 1048576 Feb  7 10:49 data3.csv

    $ echo "1,2,3,4,5" > data4.csv

    $ srun -N 1 --pty sarus run --mount=type=bind,source=/input_data,destination=/input,readonly -t debian bash
    $ ls -l /input
    -rw-r--r--. 1 root 0 1048576 Feb  7 10:49 data1.csv
    -rw-r--r--. 1 root 0 1048576 Feb  7 10:49 data2.csv
    -rw-r--r--. 1 root 0 1048576 Feb  7 10:49 data3.csv
    -rw-r--r--. 1 root 0      10 Feb  7 10:52 data4.csv

    $ cat /input/data4.csv
    1,2,3,4,5

    $ touch /input/data5.csv
    touch: cannot touch '/input/data5.csv': Read-only file system

    $ exit

.. note::

    **Bind-mounting FUSE filesystems into Sarus containers**

    By default, all FUSE filesystems are accessible only by the user who mounted them;
    this restriction is enforced by the kernel itself. Sarus is a privileged application setting up
    the container as the root user in order to perform some specific actions.

    To allow Sarus to access a FUSE mount point on the host, in order to bind mount it into a container,
    use the FUSE option ``allow_root`` when creating the mount point.
    For example, when creating an `EncFS <https://vgough.github.io/encfs/>`_ filesystem:

    .. code-block:: bash

        $ encfs -o allow_root --nocache $PWD/encfs.enc/ /tmp/encfs.dec/
        $ sarus run -t --mount=type=bind,src=/tmp/encfs.dec,dst=/var/tmp/encfs ubuntu ls -l /var/tmp

    It is possible to pass ``allow_root`` if the option ``user_allow_other`` is defined in
    ``/etc/fuse.conf``, as stated in the `FUSE manpage <https://man7.org/linux/man-pages/man8/fuse.8.html>`_.

.. _user-device-mounts:

Mounting custom devices into the container
------------------------------------------

Devices can be made available inside containers through the ``--device`` option
of :program:`sarus run`. The option can be entered multiple times, specifying
one device per option. By default, device files will be mounted into the
container at the same path they have on the host. A different destination path
can be entered using a colon (``:``) as separator from the host path.
All paths used in the option value must be absolute:

.. code-block:: bash

    $ srun sarus run --device=/dev/fuse debian ls -l /dev/fuse
    crw-rw-rw-. 1 root root 10, 229 Aug 17 17:54 /dev/fuse

    $ srun sarus run --device=/dev/fuse:/dev/container_fuse debian ls -l /dev/container_fuse
    crw-rw-rw-. 1 root root 10, 229 Aug 17 17:54 /dev/container_fuse

When working with device files, the ``--mount`` option should not be used since
access to custom devices is disabled by default through the container's device
cgroup. The ``--device`` option, on the other hand, will also whitelist the
requested devices in the cgroup, making them accessible within the container.
By default, the option will grant read, write and mknod permissions to devices;
this behavior can be controlled by adding a set of flags at the end of an option
value, still using a colon as separator. The flags must be a combination of the
characters ``rwm``, standing for *read*, *write* and *mknod* access respectively;
the characters may come in any order, but must not be repeated. The access flags
can be entered independently from the presence of a custom destination path.

The full syntax of the option is thus ``--device=host-device[:container-device][:permissions]``

The following example shows how to mount a device with read-only access:

.. code-block:: bash

    $ srun --pty sarus run -t --device=/dev/example:r debian bash

    $ echo "hello" > /dev/example
    bash: /dev/example: Operation not permitted

    $ exit

.. important::

    Sarus and the ``--device`` option cannot grant more permissions to a device
    than those which have been allowed on the host.
    For example, if in the host a device is set for read-only access, then Sarus
    cannot enable write or mknod access.

    This is enforced by the implementation of device cgroups in the Linux kernel.
    For more details, please refer to the `kernel documentation
    <https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v1/devices.html>`_.

.. _user-entrypoint-default-args:

Image entrypoint and default arguments
--------------------------------------

Sarus fully supports image entrypoints and default arguments as defined by the
`OCI Image Specification <https://github.com/opencontainers/image-spec>`_.

The entrypoint of an image is a list of arguments that will be used as the
command to execute when the container starts; it is meant to create an image
that will behave like an executable file.

The image default arguments will be passed to the entrypoint if no argument is
provided on the commmand line when launching a container. If the entrypoint is
not present, the first default argument will be treated as the executable to
run.

When creating container images with Docker, the entrypoint and default arguments
are set using the `ENTRYPOINT
<https://docs.docker.com/engine/reference/builder/#entrypoint>`_ and `CMD
<https://docs.docker.com/engine/reference/builder/#cmd>`_ instructions
respectively in the Dockerfile. For example, this file will generate an image
printing arguments to the terminal by default::

    FROM debian:stretch

    ENTRYPOINT ["/bin/echo"]
    CMD ["Hello world"]

After building such image (we'll arbitrarily call it ``echo``) and importing it
into Sarus, we can run it without passing any argument:

.. code-block:: bash

    $ srun sarus run <image repo>/echo
    Hello world

Entering a command line argument will override the default arguments passed to
the entrypoint:

.. code-block:: bash

    $ srun sarus run <image repo>/echo Foobar
    Foobar

The image entrypoint can be changed by providing a value to the ``--entrypoint``
option of :program:`sarus run`. It is important to note that when changing the
entrypoint the default arguments get discarded as well:

.. code-block:: bash

    $ srun sarus run --entrypoint=cat <image repo>/echo /etc/os-release
    PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
    NAME="Debian GNU/Linux"
    VERSION_ID="9"
    VERSION="9 (stretch)"
    ID=debian
    HOME_URL="https://www.debian.org/"
    SUPPORT_URL="https://www.debian.org/support"
    BUG_REPORT_URL="https://bugs.debian.org/"

The entrypoint can be removed by passing an empty value to ``--entrypoint``. This
is useful, for example, for inspecting and debugging containers:

.. code-block:: bash

    $ srun --pty sarus run --entrypoint "" -t <image repo>/echo bash
    $ env | grep ^PATH
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

    $ exit

.. note::

    Using the "adjacent value" style to remove an image entrypoint
    (i.e. ``--entrypoint=""``) is not supported. Please pass the empty string
    value separated by a whitespace.


.. _user-working-dir:

Working directory
-----------------

The working directory inside the container can be controlled using the
``-w/--workdir`` option of the :program:`sarus run` command:

.. code-block:: bash

    $ srun -N 1 --pty sarus run --workdir=/path/to/workdir -t debian bash

If the path does not exist, it is created inside the container.

If the ``-w/--workdir`` option is not specified but the image defines
a working directory, the container process will start
there. Otherwise, the process will start in the container's root directory (``/``).
Using image-defined working directories can be useful, for example, for
simplifying the command line when launching containers.

When creating images with Docker, the working directory is set using the
`WORKDIR <https://docs.docker.com/engine/reference/builder/#workdir>`_
instruction in the Dockerfile.

.. _user-private-pid:

PID namespace
-------------

The PID namespace for the container can be controlled through the ``--pid``
option of :program:`sarus run`. Currently, the supported values for the option
are:

* ``host``: use the host's PID namespace for the container. This allows to
  transparently support MPI implementations relying on the ranks having
  different PIDs when running on the same physical host, or using shared memory
  technologies like Cross Memory Attach (CMA); (**default**)
* ``private``: create a new PID namespace for the container. Having a private
  PID namespace can be also referred as "using PID namespace isolation" or
  simply "using PID isolation".

  .. code-block::

      $ sarus run --pid=private alpine:3.14 ps -o pid,comm
      PID   COMMAND
          1 ps


.. note::
   Consider using an :ref:`init process <user-init-process>` when running with
   a private PID namespace if you need to handle signals or run many processes
   into the container.

.. _user-init-process:

Adding an init process to the container
---------------------------------------

By default, Sarus only executes the user-specified application within the container.
When using a :ref:`private PID namespace <user-private-pid>`, the container
process is assigned PID 1 in the new namespace. The PID 1 process has unique
features in Linux: most notably, the process will ignore signals by default and
zombie processes will not be reaped inside the container (see
`[1] <https://blog.phusion.nl/2015/01/20/docker-and-the-pid-1-zombie-reaping-problem/>`_ ,
`[2] <https://hackernoon.com/the-curious-case-of-pid-namespaces-1ce86b6bc900>`_ for further reference).

If you need to handle signals or reap zombie processes (which can be useful when
executing several different processes in long-running containers), you can use the
``--init`` option to run an init process inside the container:

.. code-block:: bash

    $ srun -N 1 sarus run --pid=private --init alpine:3.14 ps -o pid,comm
    PID   COMMAND
        1 init
        7 ps

Sarus uses `tini <https://github.com/krallin/tini>`_ as its default init process.

.. warning::
   Some HPC applications may be subject to performance losses when run with an init process.
   Our internal benchmarking tests with `tini <https://github.com/krallin/tini>`_ showed
   overheads of up to 2%.

.. _user-oci-annotations:

Setting OCI annotations
-----------------------

`OCI annotations <https://github.com/opencontainers/runtime-spec/blob/main/config.md#annotations>`_
are defined in the OCI Runtime Specification as a mean to provide arbitrary
metadata for the container in the form of a key-value map. Annotation keys usually
express a hierarchical namespace structure, with domains separated by ``.``
(full stop) characters.

Annotations can be useful to control how Sarus selects OCI hooks or, depending on
their implementation, certain features of OCI hooks and OCI runtimes.

To set an annotation when running a container, use the ``--annotation`` option
of :program:`sarus run`.
The option can be passed multiple times, defining one annotation per option.
The first occurring ``=`` (equals sign) character in the option value is treated
as the separator between the annotation key and its value.

.. code-block:: bash

    $ srun -N 1 sarus run --annotation com.documentation.example.key=value debian true

Annotations set from the Sarus command line take precedence over other annotations,
for example coming from the container image (sometimes also known as "image labels")
or created automatically by Sarus itself.
For example, the hooks shipped alongside Sarus use the ``com.hooks.logging.level``
annotation to determine their verbosity level. The annotation is created internally
by Sarus to match the verbosity of the engine. A different verbosity level for the
hooks can be defined by overriding the annotation as follows:

.. code-block:: bash

    # Set Sarus hooks to print debug-level messages (level 0)
    # while Sarus keeps its normal verbosity
    $ srun -N 1 sarus run --annotation=com.hooks.logging.level=0 debian true

Please refer to your site documentation or your system administrator to learn
about custom features related to OCI annotations on a specific system.

Verbosity levels and help messages
----------------------------------

To run a command in verbose mode, enter the ``--verbose`` global option before
the command:

.. code-block:: bash

    $ srun sarus --verbose run debian:latest cat /etc/os-release

To run a command printing extensive details about internal workings, enter the ``--debug`` global
option before the command:

.. code-block:: bash

    $ srun sarus --debug run debian:latest cat /etc/os-release

To print a general help message about Sarus, use ``sarus --help``.

To print information about a command (e.g. command-specific options), use
``sarus help <command>``:

.. code-block:: bash

    $ sarus help run
    Usage: sarus run [OPTIONS] REPOSITORY[:TAG] [COMMAND] [ARG...]

    Run a command in a new container

    Note: REPOSITORY[:TAG] has to be specified as
          displayed by the "sarus images" command.

    Options:
      --centralized-repository  Use centralized repository instead of the local one
      -t [ --tty ]              Allocate a pseudo-TTY in the container
      --entrypoint arg          Overwrite the default ENTRYPOINT of the image
      --mount arg               Mount custom directories into the container
      -m [ --mpi ]              Enable MPI support
      --ssh                     Enable SSH in the container


Support for container customization through hooks
=================================================

Sarus allows containers to be customized by other programs or scripts
leveraging the interface defined by the Open Container Initiative Runtime
Specification for POSIX-platform hooks (OCI hooks for short). These
customizations are especially amenable to HPC use cases, where the dedicated
hardware and highly-tuned software adopted by high-performance systems are in
contrast with the infrastructure-agnostic nature of software containers.
OCI hooks provide a solution to open access to these resources inside containers.

The hooks which is possible to enable on a given Sarus installation are
configured by the system administrators. The :program:`sarus hooks` command can
be used to list the currently configured hooks:

.. code-block:: bash

    $ sarus hooks
    NAME                               PATH                                            STAGES
    03-nvidia-container-runtime-hook   /usr/bin/nvidia-container-runtime-hook          prestart
    05-mpi-hook                        /opt/sarus/default/bin/mpi_hook                 prestart
    07-ssh-hook                        /opt/sarus/default/bin/ssh_hook                 prestart
    09-slurm-global-sync-hook          /opt/sarus/default/bin/slurm_global_sync_hook   prestart

Please refer to your site documentation or your system administrator for information
about the conditions to enable hooks on a specific system.

The following subsections illustrate a few cases of general interest for HPC from
an end-user perspective.

.. _user-mpi-hook:

Native MPI support (MPICH-based)
--------------------------------

Sarus comes with a hook able to import native MPICH-based MPI implementations
inside the container. This is useful in case the host system features a
vendor-specific or high-performance MPI stack based on MPICH (e.g. Intel MPI,
Cray MPT, MVAPICH) which is required to fully leverage a high-speed
interconnect.

To take advantage of this feature, the MPI installed in the container (and
dynamically linked to your application) needs to be :doc:`ABI-compatible
</user/abi_compatibility>` with the MPI on the host system. Taking as an example
the Piz Daint Cray XC50 supercomputer at CSCS, to best meet the required ABI
compatibility we recommend that the container application uses one of the
following MPI implementations:

- `MPICH v3.1.4 <http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz>`_ (Feburary 2015)
- `MVAPICH2 2.2 <http://mvapich.cse.ohio-state.edu/download/mvapich/mv2/mvapich2-2.2.tar.gz>`_ (September 2016)
- Intel MPI Library 2017 Update 1

The following is an example Dockerfile to create a Debian image with MPICH
3.1.4:

.. code-block:: docker

   FROM debian:jessie

   RUN apt-get update && apt-get install -y \
           build-essential             \
           wget                        \
           --no-install-recommends     \
       && rm -rf /var/lib/apt/lists/*

   RUN wget -q http://www.mpich.org/static/downloads/3.1.4/mpich-3.1.4.tar.gz \
       && tar xf mpich-3.1.4.tar.gz \
       && cd mpich-3.1.4 \
       && ./configure --disable-fortran --enable-fast=all,O3 --prefix=/usr \
       && make -j$(nproc) \
       && make install \
       && ldconfig \
       && cd .. \
       && rm -rf mpich-3.1.4 \
       && rm mpich-3.1.4.tar.gz

.. note::

    Applications that are statically linked to MPI libraries
    will not work with the native MPI support provided by the hook.

Once the system administrator has configured the hook, containers with native
MPI support can be launched by passing the ``--mpi`` option to the
:program:`sarus run` command, e.g.:

.. code-block:: bash

    $ srun -N 16 -n 16 sarus run --mpi <repo name>/<image name> <mpi_application>

If multiple hooks are configured in the system, the ``--mpi-type`` option of
:program:`sarus run` can be used to choose a specific hook, for example:

.. code-block:: bash

    $ srun -N 16 -n 16 sarus run --mpi-type=mpich-libfabric <repo name>/<image name> <mpi_application>

When multiple hooks are configured, the system administrator has the capability
to define one of them as the default, which could be accessed through just the
``--mpi`` option. This provides compatibility with workflows which do not use
the ``--mpi-type`` option for reasons of portability or legacy.

The MPI-related hooks configured on the system can be listed through the
``--mpi`` option of the :program:`sarus hooks` command, for example:

.. code-block:: bash

    $ sarus hooks --mpi
    NAME                    MPI TYPE
    05-mpich-hook           ^mpich$ (default)
    051-mpich-ofi-hook      ^mpich-libfabric$

If the value passed to the ``sarus run --mpi-type`` option matches one of the
regular expressions under the ``MPI TYPE`` column, the corresponding hook is
enabled. The ``(default)`` qualifier alongside an MPI type indicates the
default MPI hook for the Sarus installation, which can be enabled just with
the ``sarus run --mpi`` option.

.. _user-nvidia-hook:

NVIDIA GPU support
------------------

NVIDIA provides access to GPU devices and their driver stacks inside OCI
containers through the `NVIDIA Container Toolkit hook
<https://github.com/NVIDIA/nvidia-container-toolkit>`_ .

When Sarus is configured to use this hook, the GPU devices to be made
available inside the container can be selected by setting the
``CUDA_VISIBLE_DEVICES`` environment variable in the host system. Such action is
often performed automatically by the workload manager or other site-specific
software (e.g. the SLURM workload manager sets ``CUDA_VISIBLE_DEVICES`` when
GPUs are requested via the `Generic Resource Scheduling plugin
<https://slurm.schedmd.com/gres.html>`_). Be sure to check the setup provided by
your computing site.

The container image needs to include a CUDA runtime that is suitable for both
the target container applications and the available GPUs in the host system. One
way to achieve this is by basing your image on one of the official Docker images
provided by NVIDIA, i.e. the Dockerfile should start with a line like this::

    FROM nvidia/cuda:8.0

.. note::

    To check all the available CUDA images provided by NVIDIA, visit
    https://hub.docker.com/r/nvidia/cuda/

When developing GPU-accelerated images on your workstation, we recommend using
`nvidia-docker <https://github.com/NVIDIA/nvidia-docker>`_ to run and test
containers using an NVIDIA GPU.

SSH connection within containers
--------------------------------

Sarus also comes with a hook which enables support for SSH connections within
containers.

When Sarus is configured to use this hook, you must first run the command
``sarus ssh-keygen`` to generate the SSH keys that will be used by the SSH
daemons and the SSH clients in the containers. It is sufficient to generate
the keys just once, as they are persistent between sessions.

It is then possible to execute a container passing the ``--ssh`` option to
:program:`sarus run`, e.g. ``sarus run --ssh <image> <command>``. Using the
previously generated the SSH keys, the hook instantiates an SSH daemon and sets
up a custom ``ssh`` binary inside each container created with the same command.

Within a container spawned with the ``--ssh`` option, it is possible to connect
into other containers by simply issuing the ``ssh`` command available in the
default search ``PATH``. E.g.:

.. code-block:: bash

    ssh <hostname of other node>

The custom ``ssh`` binary takes care of using the proper keys and
non-standard port in order to connect to the remote container.

When the ``ssh`` program is called without a command argument, it opens
a login shell into the remote container. In this situation, the SSH hook attempts
to reproduce the environment variables which were defined upon the launch
of the remote container. The aim is to replicate the experience of actually
accessing a shell in the container as it was created.

The hook supports the annotation ``com.hooks.ssh.authorize_ssh_key``, which allows
the user to add a public key to the container's ``authorized_keys`` file, e.g.

.. code-block:: bash

   sarus run --ssh --annotation com.hooks.ssh.authorize_ssh_key=$HOME/.ssh/<key>.pub <image>

Notice that the annotation value must be a public key file, not the public key
itself. The annotation allows remote access via SSH to the running container
through user-specified (and potentially ephemeral) keys.

.. warning::
   The SSH hook currently does not implement a poststop functionality and
   requires the use of a private PID namespace to cleanup the Dropbear daemon.
   Thus, the hook currently requires the use of a :ref:`private PID namespace <user-private-pid>`
   for the container. Thus, the ``--ssh`` option of :program:`sarus run` implies
   ``--pid=private``, and is incompatible with the use of ``--pid=host``.

OpenMPI communication through SSH
---------------------------------

The MPICH-based MPI hook described above does not support OpenMPI libraries.
As an alternative, OpenMPI programs can communicate through SSH connections
created by the SSH hook.

To run an OpenMPI program using the SSH hook, we need to manually provide a list
of hosts and explicitly launch ``mpirun`` only on one node of the allocation.
We can do so with the following commands:

.. code-block:: bash

   salloc -C gpu -N4 -t5
   srun hostname > $SCRATCH/hostfile
   srun sarus run --ssh \
        --mount=src=/users,dst=/users,type=bind \
        --mount=src=$SCRATCH,dst=$SCRATCH,type=bind \
        ethcscs/openmpi:3.1.3  \
        bash -c 'if [ $SLURM_PROCID -eq 0 ]; then mpirun --hostfile $SCRATCH/hostfile -npernode 1 /openmpi-3.1.3/examples/hello_c; else sleep 10; fi'

Upon establishing a remote connection, the SSH hook provides a ``$PATH`` covering the
most used default locations: ``/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin``.
If your OpenMPI installation uses a custom location, consider using an absolute
path to ``mpirun`` and the ``--prefix`` option as advised in the
`official OpenMPI FAQ <https://www.open-mpi.org/faq/?category=running#mpirun-prefix>`_.

Glibc replacement
-----------------

Sarus's source code includes a hook able to inject glibc libraries from the
host inside the container, overriding the glibc of the container.

This is useful when injecting some host resources (e.g. MPI libraries) into the
container and said resources depend on a newer glibc than the container's one.

The host glibc stack to be injected is configured by the system administrator.

If Sarus is configured to use this hook, the glibc replacement can be activated
by passing the ``--glibc`` option to :program:`sarus run`. Since native MPI
support is the most common occurrence of host resources injection, the hook is
also implicitly activated when using the ``--mpi`` option.

Even when the hook is configured and activated, the glibc libraries in the
container will only be replaced if the following conditions apply:

* the container's libraries are older than the host's libraries;

* host and container glibc libraries have the same soname and are ABI compatible.

.. _user-no-native-mpi:

Running MPI applications without the native MPI hook
====================================================

The :ref:`MPI replacement mechanism <user-mpi-hook>` controlled by the ``--mpi``
option is not mandatory to run distributed applications in Sarus containers. It
is possible to run containers using the MPI implementation embedded in the image,
foregoing the performance of custom high-performance hardware.

This can be useful in a number of scenarios:

* the software stack in the container should not be altered in any way
* non-performance-critical testing
* impossibility to satisfy ABI compatibility for native hardware acceleration

Sarus can be launched as a normal MPI program, and the execution context will be
propagated to the container application:

.. code-block:: bash

   mpiexec -n <number of ranks> sarus run <image> <MPI application>

The important aspect to consider is that the process management system from the
host must be able to communicate with the MPI libraries in the container.

MPICH-based MPI implementations by default use the
`Hydra process manager <https://wiki.mpich.org/mpich/index.php/Hydra_Process_Management_Framework>`_
and the `PMI-2 interface <https://link.springer.com/chapter/10.1007/978-3-642-15646-5_4>`_
to communicate between processes. OpenMPI by default uses the
`OpenRTE (ORTE) framework <https://www.open-mpi.org/papers/euro-pvmmpi-2005-orte/>`_
and the `PMIx interface <https://pmix.org/>`_, but can be configured to
support PMI-2.

.. note::

    Additional information about the support provided by PMIx for
    containers and cross-version use cases can be found here:
    https://openpmix.github.io/support/faq/how-does-pmix-work-with-containers

As a general rule of thumb, corresponding MPI implementations (e.g. using an
MPICH-compiled ``mpiexec`` on the host to launch Sarus containers featuring
MPICH libraries) should work fine together. As mentioned previously, if an
OpenMPI library in the container has been configured to support PMI, the
container should also be able to communicate with an MPICH-compiled ``mpiexec``
from the host.

The following is a minimal Dockerfile example of building OpenMPI 4.0.2 with
PMI-2 support on Ubuntu 18.04:

.. code-block:: docker

   FROM ubuntu:18.04

   RUN apt-get update && apt-get install -y \
           build-essential \
           ca-certificates \
           automake \
           autoconf \
           libpmi2-0-dev \
           wget \
           --no-install-recommends \
       && rm -rf /var/lib/apt/lists/*

   RUN wget https://download.open-mpi.org/release/open-mpi/v4.0/openmpi-4.0.2.tar.gz \
       && tar xf openmpi-4.0.2.tar.gz \
       && cd openmpi-4.0.2 \
       && ./configure --prefix=/usr --with-pmi=/usr/include/slurm-wlm --with-pmi-libdir=/usr/lib/x86_64-linux-gnu CFLAGS=-I/usr/include/slurm-wlm \
       && make -j$(nproc) \
       && make install \
       && ldconfig \
       && cd .. \
       && rm -rf openmpi-4.0.2.tar.gz openmpi-4.0.2

When running under the Slurm workload manager, the process management interface
can be selected with the ``--mpi`` option to ``srun``. The following example
shows how to run the :doc:`OSU point-to-point latency test </cookbook/osu_mb/osu_mb>`
from the Sarus cookbook on CSCS' Piz Daint Cray XC50 system without native
interconnect support:

.. code-block:: bash

   $ srun -C gpu -N2 -t2 --mpi=pmi2 sarus run ethcscs/mpich:ub1804_cuda92_mpi314_osu ./osu_latency
   ###MPI-3.0
   # OSU MPI Latency Test v5.6.1
   # Size          Latency (us)
   0                       6.82
   1                       6.80
   2                       6.80
   4                       6.75
   8                       6.79
   16                      6.86
   32                      6.82
   64                      6.82
   128                     6.85
   256                     6.87
   512                     6.92
   1024                    9.77
   2048                   10.75
   4096                   11.32
   8192                   12.17
   16384                  14.08
   32768                  17.20
   65536                  29.05
   131072                 57.25
   262144                 83.84
   524288                139.52
   1048576               249.09
   2097152               467.83
   4194304               881.02

Notice that an ``--mpi=pmi2`` option was passed to ``srun`` but *not* to
:program:`sarus run`.

Remote development with Visual Studio Code
==========================================

.. |vscode_button| image:: vscode_remote_window_button.png
                   :alt: Visual Studio Code remote window button

`Visual Studio Code <https://code.visualstudio.com/>`_ is a popular Programming
IDE that can be configured to edit and test code on remote systems.
Remote systems can be accessed via SSH protocol, hence the Sarus SSH hook can be
used to extend the IDE to work on remote Sarus container environments.

Follow these steps to configure Visual Studio Code to access remote Sarus 
instances:

On the remote system start a Sarus container enabling SSH:

1. Copy your public SSH key on the remote host that will run the container.

   .. code-block:: bash

      <me>@<laptop>:~$ scp ~/.ssh/id_ed25519.pub <remote_user>@<remote_node>:~/.ssh/

2. Run the remote container with the ``--ssh`` option and the annotation to authorize 
   your public key.

   .. code-block:: bash

      <remote_user>@<remote_node>:~> sarus run --ssh --tty --entrypoint bash \
      > --annotation com.hooks.ssh.authorize_ssh_key=$HOME/.ssh/id_ed25519.pub \
      > <container_image>

3. Test that you can now access the remote container via SSH on port 15263.

   .. code-block:: bash

      <me>@<laptop>:~$ ssh -o StrictHostKeyChecking=no -o ControlMaster=no \
      > -o UserKnownHostsFile=/dev/null -p 15263 <remote_user>@<remote_node>

 
Configure Visual Studio Code to access the remote Sarus container:

4. On Visual Studio Code, install "Remote Development" and "Remote - SSH" 
   extensions.

5. Click on the bottom left corner |vscode_button| symbol on Visual Studio Code.

6. Select "Connect to Host..." .

7. Select "+ Add New SSH Host..." .

8. On the "Enter SSH Connection Command" window, insert the SSH command line that 
   you tested on step 3 .

9. Select the SSH client configuration file where you prefer to add the host,
   i.e. ``$HOME/.ssh/config``.

10. Now on this file you should find a new entry like:

   .. code-block:: bash

      Host <remote_node>
          HostName <remote_node>
          StrictHostKeyChecking no
          ControlMaster no
          UserKnownHostsFile /dev/null
          Port 15263
          User <remote_user>

11. Select "Connect" to connect the IDE to the remote container environment.

For more details about "Remote - SSH" Visual Studio Code extension, you can
refer to `this tutorial <https://code.visualstudio.com/docs/remote/ssh-tutorial>`_ 
from the official Visual Studio Code documentation.

