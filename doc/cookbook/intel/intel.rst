*************************
Intel Cluster Edition 19.01
*************************

This section provides the instructions to build Intel Parallel Studio 2019 Update 1 based on the Centos 7 distribution: Intel Parallel Studio provides a complete environment to build an optimised appication, since it features compilers, communication and mathematical libraries.

Please note that the Intel license prevents the redistribution of the software, therefore the license file needs to be available locally on the system where you are building the image:
the license file is called `intel_license_file.lic` in the example Dockerfile provided below and it will be copied inside the container during the installation.

Furthermore, you need to create a local configuration file with the instructions required to proceed with the silent installation of Intel Parallel Studio: 
the name of the file is `intel19.01_silent.cfg` in the example below and it will provide the installation of the Intel C, C++ and Fotran Compiler together with the Intel MPI library and the Intel MKL.


Container image and Dockerfile
==============================
The starting point is the official image of CentOS 7: on top of that image, you need to install the required build tools and proceed with the silent installation of the Intel compiler.

.. code-block:: docker

   FROM centos:7
   
   SHELL ["/bin/bash", "--login", "-c"]
   
   RUN yum install -y gcc gcc-c++ which make wget strace cpio
   
   WORKDIR /usr/local/src
   
   # install Intel Compiler and Intel MPI
   COPY intel_license_file.lic /opt/intel/licenses/USE_SERVER.lic
   ADD intel19.01_silent.cfg .
   ADD parallel_studio_xe_2019_update1_cluster_edition_online.tgz .
   
   RUN cd parallel_studio_xe_2019_update1_cluster_edition_online \
       && ./install.sh --ignore-cpu -s /usr/local/src/intel19.01_silent.cfg
   
   RUN echo -e "source /opt/intel/bin/compilervars.sh intel64 \nsource /opt/intel/mkl/bin/mklvars.sh intel64 \nsource /opt/intel/impi/2019.1.144/intel64/bin/mpivars.sh release_mt" >> /etc/profile.d/sh.local \
       && echo -e "/opt/intel/lib/intel64 \n/opt/intel/mkl/lib/intel64 \n/opt/intel/impi/2019.1.144/intel64/lib \n/opt/intel/impi/2019.1.144/intel64/lib/release_mt \n/opt/intel/impi/2019.1.144/intel64/libfabric/lib \n/opt/intel/impi/2019.1.144/intel64/libfabric/lib/prov" > /etc/ld.so.conf.d/intel.conf \
       && ldconfig
   
   ENV PATH /opt/intel/impi/2019.1.144/intel64/bin:/opt/intel/bin:$PATH

Please note that the login shell will source the profile files and therefore update the paths to retrieve compiler executables and libraries: 
however it will have an effect only on the commands executed within the Dockerfile, serving only as a reminder when building applications with the Intel image.

The instructions above will copy the local license file `intel_license_file.lic` to `/opt/intel/licenses/USE_SERVER.lic`, since the license file of the example will use a server to authenticate. 
The verification of the license used to fail otherwise: please adapt the verification settings to your specific license and be aware that the number of users accessing a server license at the same time is usually limited.

The following silent configuration file provides a minimal list of the Intel COMPONENTS necessary for the installation: the advantage of the minimal list is the reduced amount of disk space required while building the image:

.. code-block:: bash

   ACCEPT_EULA=accept
   CONTINUE_WITH_OPTIONAL_ERROR=yes
   PSET_INSTALL_DIR=/opt/intel
   CONTINUE_WITH_INSTALLDIR_OVERWRITE=yes
   COMPONENTS=;intel-clck__x86_64;intel-icc__x86_64;intel-ifort__x86_64;intel-mkl-core-c__x86_64;intel-mkl-cluster-c__noarch;intel-mkl-core-f__x86_64;intel-mkl-cluster-f__noarch;intel-mkl-f__x86_64;intel-imb__x86_64;intel-mpi-sdk__x86_64
   PSET_MODE=install
   ACTIVATION_LICENSE_FILE=/opt/intel/licenses/USE_SERVER.lic
   ACTIVATION_TYPE=license_server
   AMPLIFIER_SAMPLING_DRIVER_INSTALL_TYPE=kit
   AMPLIFIER_DRIVER_ACCESS_GROUP=vtune
   AMPLIFIER_DRIVER_PERMISSIONS=666
   AMPLIFIER_LOAD_DRIVER=no
   AMPLIFIER_C_COMPILER=none
   AMPLIFIER_KERNEL_SRC_DIR=none
   AMPLIFIER_MAKE_COMMAND=/usr/bin/make
   AMPLIFIER_INSTALL_BOOT_SCRIPT=no
   AMPLIFIER_DRIVER_PER_USER_MODE=no
   SIGNING_ENABLED=yes
   ARCH_SELECTED=INTEL64
