********
Glossary
********

This page contains the definitions of technical terms and concepts as they are
used in the scope of Sarus and throughout its documentation.

.. glossary::

    container
        A Linux process isolated from the rest of the system through
        `Linux namespaces <https://man7.org/linux/man-pages/man7/namespaces.7.html>`_
        and, potentially, other technologies like
        `cgroups <https://man7.org/linux/man-pages/man7/cgroups.7.html>`_,
        `SELinux <http://selinuxproject.org/page/Main_Page>`_ or
        `AppArmor <https://wiki.ubuntu.com/AppArmor>`_.

        Its filesystem and execution context primarily come from a
        :term:`container image`.

        A container can be thought as the runtime instantiation of a container image.

    container engine
        A software that interacts with users, imports
        :term:`container images <container image>`, and, from the point of view
        of the users, runs :term:`containers <container>`.

        When it comes to container execution, engines are generally tasked with
        preparing the mount point and metadata for the container, combining
        the contents of the image with additional instructions provided by user
        input or by system administrator settings.

        Under the hood, many engines delegate actual container creation and
        running to a :term:`container runtime`.

        Sarus is technically defined as a container engine.

    container image
        A standalone, portable, self-sufficient unit of software which provides
        filesystem content and metadata for the creation of a :term:`container`.

    container runtime
        A program that interacts at low level with the kernel to create, start
        or modify containers.

        The container runtime usually consumes the mount point and metadata
        provided by a :term:`container engine`. For testing purposes, a runtime
        can also be used directly, with individually crafted filesystem and
        metadata for a container.

        Sarus uses `runc <https://github.com/opencontainers/runc>`_ as its
        default container runtime.

    digest
        An immutable identifier which uniquely points to a specific version of an
        image within a :term:`repository`.

        As defined by the
        `OCI Image Specification <https://github.com/opencontainers/image-spec/blob/main/descriptor.md#digests>`_,
        digests take the form of a string using the ``<algorithm>:<encoded>``
        pattern. The ``algorithm`` portion indicates the cryptographic algorithm
        used for the digest, while the ``encoded`` portion represents the result of
        the hash function.

        In an `OCI image <https://github.com/opencontainers/image-spec/blob/main/spec.md#overview>`_,
        every element is identified by a digest. The digest of the
        :term:`image manifest` is considered as the digest of the image
        itself, and therefore also referred to as "image digest".

        In the scope of Sarus, the "image digest" is the digest of the image
        **as stored in the** :term:`registry` **where it was pulled from**. This
        is meant to help users relate local and remote images.

    image manifest
        In a Docker or OCI image, the manifest is a JSON file listing the elements
        which compose the container image, that is the filesystem layers and image
        configuration file.

        A manifest describes a single image for a specific architecture and
        operating system.

        For the formal definition of the OCI Image Manifest, please refer to
        `this page <https://github.com/opencontainers/image-spec/blob/main/manifest.md>`_.

    image reference
        A string referencing a :term:`container image` within a
        :term:`registry` or :term:`repository`.
        An image reference takes the following form::

            [[server/]namespace/]image[:tag][@digest]

        The ``server`` component represents the location of the registry to which the
        image belongs. This component is similar to a URL, but does not contain a
        protocol specifier; it can specify a port number, separated by a colon
        (for example ``test.registry.io:5000``). When a ``server`` component is
        not specified in the command line, Sarus defaults to ``docker.io``
        (Docker Hub).

        The ``namespace`` component represents a namespace which may exist within
        a given registry and is used to subdivide or categorize images. Namespaces
        might be used by registries to represent organizations, user groups or
        individual users. Multiple nested namespaces can be specified by using the
        ``/`` character as separator. When a ``namespace`` component is
        not specified in the command line, Sarus defaults to ``library``, which is
        the namespace for official images in Docker Hub.

        The ``image`` component represents the name of the repository where the image
        is stored. By extension, this may also be referred to as the "image name".

        The group ``server/namespace/image`` is also known as "full name"
        or "fully qualified image name".
        On the other hand, when the ``server`` or ``namespace`` components are not
        present, the group is known as "short name" or "unqualified image name".

        The ``tag`` component represents a :term:`tag`.
        When a ``tag`` component is not specified in the command line, Sarus
        defaults to ``latest``.

        The ``digest`` component represents a :term:`digest`.

        Formally, an image reference is defined by the following grammar::

            reference                       := name [ ":" tag ] [ "@" digest ]
            name                            := [domain '/'] path-component ['/' path-component]*
            domain                          := domain-component ['.' domain-component]* [':' port-number]
            domain-component                := /([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9-]*[a-zA-Z0-9])/
            port-number                     := /[0-9]+/
            path-component                  := alpha-numeric [separator alpha-numeric]*
            alpha-numeric                   := /[a-z0-9]+/
            separator                       := /[_.]|__|[-]*/

            tag                             := /[\w][\w.-]{0,127}/

            digest                          := digest-algorithm ":" digest-hex
            digest-algorithm                := digest-algorithm-component [ digest-algorithm-separator digest-algorithm-component ]*
            digest-algorithm-separator      := /[+.-_]/
            digest-algorithm-component      := /[A-Za-z][A-Za-z0-9]*/
            digest-hex                      := /[0-9a-fA-F]{32,}/ ; At least 128 bit digest value

    .. _glossary-registry:

    registry
        A hosted service (usually remote or cloud-based) which contains :term:`repositories <repository>`
        of images. A registry responds to an API which enables clients to query and
        exchange data with the registry. The most popular examples of registry APIs
        are the `Docker Registry HTTP API V2 <https://docs.docker.com/registry/spec/api/>`_
        and the `OCI Distribution Spec API <https://github.com/opencontainers/distribution-spec/blob/main/spec.md>`_.

    repository
        A collection of container images, usually representing a similar software stack
        or serving a similar purpose. Repositories can be hosted remotely in a
        :term:`registry` or in a local filesystem.

        Different images in a repository can be labeled with a :term:`tag`.

    tag
        A string used to label an :term:`image <container image>` within a
        :term:`repository`, in order to differentiate it from other images.
        Tags are mutable, and a tag can point to different images at different
        moments in time. In this regard, tags are conceptually similar to Git branches.

        Many container tools (including Sarus) use the tag ``latest`` as default
        when a specific tag is not provided to an operation involving an image.

        Tags are often used to identify specific software versions, releases or
        branches.
        Conventionally, the tag ``latest`` points to the latest software version in
        the repository, but it may also represent the latest *stable* software
        version.


Further reading
===============

- `A Practical Introduction to Container Terminology (Red Hat Developer portal)
  <https://developers.redhat.com/blog/2018/02/22/container-terminology-practical-introduction>`_

- `Docker glossary <https://docs.docker.com/glossary/>`_
