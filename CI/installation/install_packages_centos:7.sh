#!/bin/bash

set -ex

# Install packages
yum install -y sudo
sudo yum install -y epel-release
sudo yum install -y centos-release-scl-rh
sudo yum install -y devtoolset-3-gcc-c++ glibc-static sudo curl wget rsync which \
    make bzip2 autoconf automake libtool squashfs-tools libcap-devel cmake3 \
    zlib-devel openssl-devel expat-devel
sudo yum clean all
sudo rm -rf /var/cache/yum

# Enable devtoolset-3 to use GCC 4.9.2
source /opt/rh/devtoolset-3/enable

# Create symlink to cmake3 to uniform commands with other distributions
sudo ln -s /usr/bin/cmake3 /usr/bin/cmake
