********
Overview
********

Sarus is a tool for High-Performance Computing (HPC) systems that provides a
user-friendly way to instantiate feature-rich containers from OCI images. It
has been designed to address the unique requirements of HPC installations, such
as: native performance from dedicated hardware, improved security due to the
multi-tenant nature of the systems, support for network parallel filesystems and
diskless computing nodes, compatibility with a workload manager or job
scheduler.

Keeping flexibility, extensibility and community efforts in high regard, Sarus
relies on industry standards and open source software. Consider for instance the
use of `runc <https://github.com/opencontainers/runc>`_, an OCI-compliant
container runtime that is also `used internally by Docker
<https://www.docker.com/blog/runc/>`_. Moreover, Sarus leverages the `Open
Containers Initiative (OCI) <https://www.opencontainers.org/>`_ specifications
to extend the capabilities of ``runc`` and enable multiple high-performance
features. In the same vein, Sarus depends on a widely-used set of libraries,
tools, and technologies to reap several benefits: reduce maintenance effort and
lower the entry barrier for service providers wishing to install the software,
or for developers seeking to contribute code.

Sarus architecture
==================

.. figure:: architecture.*
   :alt: Sarus architecture

   Sarus architecture diagram

The workflows supported by Sarus are implemented through the interaction of
several software components.

The **CLI** component processes the program
arguments entered through the command line and calls other components to perform
the actions requested by the user.

The **Image Manager** component is responsible for:

* coordinating the import of container images onto the system (making use of
  utilities like `Skopeo <https://github.com/containers/skopeo>`_ and
  `Umoci <https://umo.ci>`_);

* converting imported images to Sarus' own format;

* storing images on local system repositories;

* querying the contents of local repositories.

The **Runtime** component creates and executes containers,
first by setting up a bundle according to the OCI Runtime Specification: such
bundle is made of a root filesystem directory for the container and a JSON
configuration file. After preparing the bundle, the Runtime component will call
an external **OCI runtime** (such as ``runc``), which will effectively spawn the
container process. Sarus can instruct the OCI runtime to use one or more
:doc:`OCI hooks </config/configure_hooks>`, small programs able to perform
actions (usually related to container customization) at selected points during
the container lifetime.

In the following sections, we will illustrate how Sarus performs some of its
fundamental operations, highlighting the motivations and purposes of the
implemented solutions.

Importing container images
==========================

One of the first actions a user will perform with a fresh Sarus installation
is getting some container images in a form readily usable by the program.
This is accomplished by either:

* retrieving images from a remote registry
  (e.g. `Docker Hub <https://hub.docker.com>`_) using the :program:`sarus pull`
  command;

* loading images from local :program:`tar` archives using the
  :program:`sarus load` command.

Sarus imports images through `Skopeo <https://github.com/containers/skopeo>`_,
a highly versatile utility able to perform operations on image repositories,
transfer container images and convert them between different formats.
Skopeo is instructed to retrieve the image data (whether located on a remote
registry or in a local file) and transform it into an OCI image directory,
that is a directory structured according to the `OCI Image Specification
<https://github.com/opencontainers/image-spec>`_.

Once the container image is available in such a standard format, the image
filesystem is *unpacked* into a plain :ref:`temporary directory <config-reference-tempDir>`
using `Umoci <https://umo.ci>`_. Umoci is a utility to create, modify and
interact with OCI images, serving also as a reference implementation of
the OCI Image Specification. The unpack operation has the additional effect of
reducing the final size of Sarus images on disk, by merging together layers and
potentially duplicated data.

The image filesystem contents are then squashed together, obtaining a
*flattened* image in the `SquashFS <https://en.wikipedia.org/wiki/SquashFS>`_
format. A metadata file is also generated from a subset of the OCI image
configuration. Squashing the image improves the I/O performance of the container,
as detailed below in :ref:`overview-instantiation-rootfs`.

After the SquashFS image and the metadata file are generated, Sarus copies them
into the local user repository, described in the next section.

Local system repositories
=========================

Sarus stores images available on the system in **local repositories**, which are
individual for each user. The application expects to access a configurable
:ref:`base path <config-reference-localRepositoryBaseDir>`, where directories
named after users are located. Sarus will look for a local repository in the
``<base path>/<user name>/.sarus`` path. If a repository does not exist, a new,
empty one is created.

A Sarus local repository is a directory containing at least:

* the *cache* directory for downloaded image data: this directory is used
  by Sarus to store downloaded individual image components (like filesystem layers
  and OCI configuration files), so they can be reused by subsequent pull commands;
* the *images* directory for Sarus images: inside this directory, images are
  stored in a hierarchy with the format ``<registry server>/<repository>/<image
  name>``, designed to replicate the structure of the strings used to
  identify images. At the end of a pull or load process, Sarus copies the
  image SquashFS and metadata files into the last folder of the hierarchy,
  named after the image, and sets the names of both files to match the image tag;
* the *metadata.json* file indexing the contents of the images folder

.. figure:: local-repository.*
   :scale: 100 %
   :alt: Structure of a Sarus local repository

   Structure of a Sarus local repository

Sarus can also be configured to create a system-wide :ref:`centralized
repository <config-reference-centralizedRepositoryDir>`. Such repository
is intended to broadcast images to users, e.g. in cases when said images cannot
be freely redistributed. The centralized repository is meant to be read-only for
regular users, and its contents should be modifiable only by the system
administrators.

Users can query the contents of the individual and centralized repositories
using the :program:`sarus images` command.

Container instantiation
=======================

The Runtime component of Sarus is responsible for setting up and coordinating
the launch of container instances. When the user requests the execution of a
container process through the :program:`sarus run` command, an OCI bundle is
first created in a :ref:`dedicated directory <config-reference-OCIBundleDir>`.
As mentioned above, an OCI bundle is defined by the OCI Runtime Specification as
the content from which an OCI-compliant low-level runtime, e.g. runc, will spawn
a container. The bundle is formed by a *rootfs* directory, containing the root
filesystem for the container, and a *config.json* file providing detailed
settings to the OCI runtime.

Before actually generating the contents of the bundle, Sarus will create and
join a new Linux mount namespace in order to make the mount points of the
container inaccessible from the host system. An :ref:`in-memory temporary
filesystem <config-reference-ramFilesystemType>` is then mounted on the
directory designated to host the OCI bundle. This process yields several
beneficial effects, e.g.:

* Unsharing the mount namespace prevents other processes of the host system from having
  visibility on any artifact related to the container instance [unshare-manpage]_ [mount-namespace-manpage]_.
* The newly-created mount namespace will be deleted once the container and Sarus
  processes exit; thus, setting up the bundle in a filesystem that belongs only to
  the mount namespace of the Sarus process ensures complete cleanup of
  container resources upon termination.
* Creating the bundle, and consequently the container rootfs, in an in-memory temporary
  filesystem improves the performance of the container writable layer. This solution also suits diskless computing nodes (e.g. as those found in Cray XC systems), where the host filesystem also resides in RAM.

In the next subsections, we will describe the generation of the bundle contents
in more detail.

.. _overview-instantiation-rootfs:

Root filesystem
---------------

The root filesystem for the container is assembled in a :ref:`dedicated
directory <config-reference-rootfsFolder>` inside the OCI bundle location
through several steps:

1. The SquashFS file corresponding to the image requested by the user is mounted as
a *loop device* on the configured rootfs mount point. The loop mount allows
access to the image filesystem as if it resided on a real block device (i.e. a
storage drive). Since Sarus images are likely to be stored on network parallel
filesystems, reading multiple different files from the image [#f1]_ causes the
thrashing of filesystem metadata, and consequently a significant performance
degradation. Loop mounting the image prevents metadata thrashing and improves
caching behavior, as all container instances access a single SquashFS file on
the parallel filesystem. The effectiveness of this approach has already been
demonstrated by Shifter [ShifterCUG2015]_.

2. Sarus proceeds to create an `overlay filesystem
<https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt>`_. An
overlay filesystem, as the name suggests, is formed by two different filesystem
layers on top of each other (thus called respectively *upper* and *lower*), but it
is presented as a single entity which combines both. The loop-mounted image is
re-used as the *read-only* lower layer, while part of the OCI bundle temporary
filesystem forms the *writable* upper layer. An overlay filesystem allows the
contents of Sarus containers to be transparently modifiable by the users, while
preserving the integrity of container images: modifications exist only in the
overlay upper filesystem, while corresponding entries in the lower filesystem
are hidden. Please refer to the official
`OverlayFS <https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt>`_
documentation for more details.

3. Selected system configuration files (e.g. ``/etc/hosts``, ``/etc/passwd``,
``/etc/group``) are copied into the rootfs of the container. These
configurations are required to properly setup core functionality of the
container in a multi-tenant cluster system, for example file permissions in
shared directories, or networking with other computing nodes.

4. *Custom mounts* are performed. These are bind mounts requested by the
:ref:`system administrator <config-reference-siteMounts>` or by the :ref:`user
<user-custom-mounts>` to customize the container according to the needs
and resources of an HPC system or a specific use case.

5. The container's rootfs is completed by finally `remounting
<http://man7.org/linux/man-pages/man2/mount.2.html>`_ the filesystem to remove
potential suid bits from all its files and directories.

.. figure:: oci-bundle.*
   :scale: 100 %
   :alt: OCI bundle setup in Sarus

   OCI bundle setup in Sarus

config.json
-----------

The JSON configuration file of the OCI bundle is generated by combining data
from the runtime execution context, command-line parameters and parameters
coming from the image. We hereby highlight the most important details:

* The uid/gid of the user from the host system are assigned to the container
  process, regardless of the user settings in the original image.
  This is done to keep a consistent experience with the host system, especially
  regarding file  ownership and access permissions.
* If the image specified an entrypoint or default arguments, these are honored,
  unless the user specifies an override through Sarus's command line. For more details,
  please refer to :ref:`this section <user-entrypoint-default-args>` of the User Guide.
* The container environment variables are created by uniting variables from
  different sources: the host environment, the image, the Sarus configuration
  file and the command line.
  Unless explicitly re-defined in the Sarus configuration file or command line,
  variable values from the image have precendence. This ensures the
  container behaves as expected by its creators (e.g. in the case of ``PATH``).
  Selected variables are also adapted by Sarus to suit system-specific
  extensions, like NVIDIA GPU support, native MPI support or container SSH connections.
* If a working directory is specified either in the image or from the Sarus CLI,
  the container process is started there. Otherwise, the process is started in
  the container's root directory.
  In this regard, Sarus shows the same behavior as Docker.
* The container process is configured to run with all Linux capabilities disabled [#f2]_,
  thus preventing it from acquiring new privileges by any means. This is done in the
  interest of security.
* A new mount namespace is setup for the container process.
* The container process runs in the PID namespace of the calling host process
  by default. A separate PID namespace for the container can be created if requested
  from the command line.
* Settings for OCI hooks are generated from the `OCI hook JSON configuration files
  <https://github.com/containers/libpod/blob/master/pkg/hooks/docs/oci-hooks.5.md>`_
  which are :doc:`configured</config/configure_hooks>` by the sysadmin.

Container launch
----------------

Once the bundle's rootfs directory and confg.json file are in place, Sarus forks
a process calling an :ref:`OCI-compliant <config-reference-runcPath>` runtime,
which in turn spawns and maintains the container process.

The OCI runtime is also in charge of executing the :doc:`OCI hooks
</config/configure_hooks>` specified by Sarus. Hooks are an effective way of
extending the functionality provided by the container runtime without additional
development or maintenance effort on the runtime itself. In the context of HPC,
hooks have shown the potential to augment containers based on open standards
with native support for dedicated custom hardware, like accelerators or
interconnect technologies, by letting vendors and third-party developers create
ad hoc hook programs.

Once the container and OCI runtime processes terminate, Sarus itself concludes
its workflow and exits.


.. [#f1] A prominent use case is, for example, a Python application.
.. [#f2] Linux divides the privileges traditionally associated with superuser into distinct units, known as `capabilities <http://man7.org/linux/man-pages/man7/capabilities.7.html>`_.

.. [unshare-manpage] http://man7.org/linux/man-pages/man2/unshare.2.html
.. [mount-namespace-manpage] http://man7.org/linux/man-pages/man7/mount_namespaces.7.html
.. [ShifterCUG2015] Jacobsen, D.M., Canon, R.S., “Contain This, Unleashing Docker for HPC”, Cray Users GroupConference 2015 (CUG’15), https://www.nersc.gov/assets/Uploads/cug2015udi.pdf
