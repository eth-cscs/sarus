#!/bin/bash
set -ex

# Install packages
sudo yum install -y epel-release
sudo yum install -y centos-release-scl-rh
sudo yum install -y devtoolset-3-gcc-c++ glibc-static sudo curl wget rsync which \
    make bzip2 autoconf automake libtool squashfs-tools libcap-devel cmake3 \
    zlib-devel openssl-devel expat-devel git
sudo yum clean all
sudo rm -rf /var/cache/yum

# Create symlink to cmake3 to uniform commands with other distributions
sudo ln -s /usr/bin/cmake3 /usr/bin/cmake

# Enable devtoolset-3 to use GCC 4.9.2
echo "source scl_source enable devtoolset-3" >> ~/.bashrc
source /opt/rh/devtoolset-3/enable

# DOCS: END
echo "source scl_source enable devtoolset-3" >> /home/docker/.bashrc
