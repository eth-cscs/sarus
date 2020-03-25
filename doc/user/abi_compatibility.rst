ABI compatibility and its implications
======================================

The :ref:`native MPICH hook <user-mpi-hook>` injects host libraries into the
container, effectively replacing the compatible MPI implementation provided by the
container image. This is done to achieve optimal performance: while container
images are ideally `infrastructure agnostic
<https://github.com/opencontainers/runtime-spec/blob/master/principles.md#3-infrastructure-agnostic>`_
to maximize portability, the interconnect technologies found on HPC systems are
often proprietary, requiring vendor-specific MPI software to be used at their
full potential, or in some cases just to access the network hardware.

In order to allow the container applications to work seamlessly after the
replacement, the container MPI libraries and the host MPI libraries must have
compatible `application binary interfaces
(ABI) <https://en.wikipedia.org/wiki/Application_binary_interface>`_.

An ABI is an interface which defines interactions between software components at
the machine code level, for example between an application executable and a
shared object library. It is conceptually opposed to an
`API (application programming interface) <https://en.wikipedia.org/wiki/Application_programming_interface>`_,
which instead defines interactions at the source code level.

If two libraries implement the same ABI, applications can be dynamically linked
to either of them and still work seamlessly.

In 2013, the developers of several MPI implementations based on the MPICH
library announced an `initiative <https://www.mpich.org/abi/>`_ to maintain ABI
compatibility between their implementations. At the time of writing, the
software adhering to such collaboration are:

* MPICH v3.1 (Released Feburary 2014)
* Intel® MPI Library v5.0 (Released June 2014)
* Cray MPT v7.0.0 (Released June 2014)
* MVAPICH2 2.0 (Release June 2014)
* Parastation MPI 5.1.7-1 (Released December 2016)
* RIKEN MPI 1.0 (Released August 2016)

Applications linked to one of the above libraries can work with any other
implementation on the list, assuming the ABI versions of the libraries linked
at compile time and linked at runtime are compatible.

The MPICH ABI Initiative establishes the `naming convention
<https://wiki.mpich.org/mpich/index.php/ABI_Compatibility_Initiative#ABI_Compliance_Requirements>`_
for complying libraries. The ABI version of a binary can be determined by the
string of numbers trailing the filename. For example, in the case of MPICH
v3.1.1, which has ABI version ``12.0.1``:

.. code-block:: bash

   $ ls -l /usr/local/lib
   total 9356
   lrwxrwxrwx. 1 root root      13 Mar 16 13:30 libfmpich.so -> libmpifort.so
   -rw-r--r--. 1 root root 5262636 Mar 16 13:30 libmpi.a
   -rwxr-xr-x. 1 root root     990 Mar 16 13:30 libmpi.la
   lrwxrwxrwx. 1 root root      16 Mar 16 13:30 libmpi.so -> libmpi.so.12.0.1
   lrwxrwxrwx. 1 root root      16 Mar 16 13:30 libmpi.so.12 -> libmpi.so.12.0.1
   -rwxr-xr-x. 1 root root 2649152 Mar 16 13:30 libmpi.so.12.0.1
   lrwxrwxrwx. 1 root root       9 Mar 16 13:30 libmpich.so -> libmpi.so
   lrwxrwxrwx. 1 root root      12 Mar 16 13:30 libmpichcxx.so -> libmpicxx.so
   lrwxrwxrwx. 1 root root      13 Mar 16 13:30 libmpichf90.so -> libmpifort.so
   -rw-r--r--. 1 root root  305156 Mar 16 13:30 libmpicxx.a
   -rwxr-xr-x. 1 root root    1036 Mar 16 13:30 libmpicxx.la
   lrwxrwxrwx. 1 root root      19 Mar 16 13:30 libmpicxx.so -> libmpicxx.so.12.0.1
   lrwxrwxrwx. 1 root root      19 Mar 16 13:30 libmpicxx.so.12 -> libmpicxx.so.12.0.1
   -rwxr-xr-x. 1 root root  185336 Mar 16 13:30 libmpicxx.so.12.0.1
   -rw-r--r--. 1 root root  789026 Mar 16 13:30 libmpifort.a
   -rwxr-xr-x. 1 root root    1043 Mar 16 13:30 libmpifort.la
   lrwxrwxrwx. 1 root root      20 Mar 16 13:30 libmpifort.so -> libmpifort.so.12.0.1
   lrwxrwxrwx. 1 root root      20 Mar 16 13:30 libmpifort.so.12 -> libmpifort.so.12.0.1
   -rwxr-xr-x. 1 root root  364832 Mar 16 13:30 libmpifort.so.12.0.1
   lrwxrwxrwx. 1 root root       9 Mar 16 13:30 libmpl.so -> libmpi.so
   lrwxrwxrwx. 1 root root       9 Mar 16 13:30 libopa.so -> libmpi.so
   drwxr-xr-x. 2 root root      39 Mar 16 13:30 pkgconfig

The initiative also defines `update rules
<https://wiki.mpich.org/mpich/index.php/ABI_Compatibility_Initiative#ABI_String_Updates>`_
for the version strings: assuming a starting version of ``libmpi.so.12.0.0``,
updates to the library implementation which do not change the public interface
would result in ``libmpi.so.12.0.1``. Conversely, the addition of new features,
without altering existing interface, would result in ``libmpi.so.12.1.0``.
Interface breaking changes would result in a bump to the first number in the
string: ``libmpi.so.13.0.0``.

Within the boundary of a compatible interface version, one could perform the
following type of replacements:

* **Forward replacement**: replacing the library an application was originally
  linked to with a *newer* version (e.g. ``12.0.x`` is replaced by ``12.1.x``).
  This is is *safe*, since the newer version maintains the interface of the
  older one. An application can still successfully access the functions and
  symbols found at compilation time.

* **Backward replacement**: replacing the library an application was originally
  linked to with an *older* version (e.g. ``12.1.x`` is replaced by ``12.0.x``).
  This is *NOT safe*, since the new features/functions which resulted in the
  minor version bump are not present in the older library. If an application
  tries to access functions which are only present in the newer version, this
  will result in an application failure (crash).

Armed with this knowledge, let's go full circle and return to our starting
point. The :ref:`native MPICH hook <user-mpi-hook>` will mount host MPI
libraries over corresponding libraries into the container to enable transparent
native performance.
Performing this action over a container image with an MPI implementation newer
than the configured host one might prevent container applications from running.
For this reason, before performing the mounts the hook verifies the ABI string
compatibility between the involved libraries.
If non-matching major version are detected, the hook will stop execution
and return an error. If non-matching minor versions are detected when performing
a backward replacement, the hook will print a warning but will proceed in the
attempt to let the container application run.

The host MPI implementation to be injected is system-specific and is
:doc:`configured by the system administrator </config/mpi-hook>`.

.. important::

    Please refer to the documentation or contacts provided by your computing
    site to learn more about the compatible MPI versions on a given system.
