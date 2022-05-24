#!/bin/bash
set -ex

# Install packages
sudo yum install -y epel-release
sudo yum install -y centos-release-scl-rh
sudo yum install -y devtoolset-11-gcc-c++ glibc-static sudo curl wget rsync which \
    make bzip2 autoconf automake libtool git squashfs-tools cmake3 ca-certificates \
    zlib-devel zlib-static runc tini-static skopeo \
    python3 python3-pip python3-setuptools
sudo yum clean all
sudo rm -rf /var/cache/yum

# Create symlink to cmake3 to uniform commands with other distributions
sudo ln -s /usr/bin/cmake3 /usr/bin/cmake
sudo ln -s /usr/bin/ctest3 /usr/bin/ctest

# Enable devtoolset-11 to use GCC 11.2.1
source /opt/rh/devtoolset-11/enable

# The following dependencies are not provided via the system's package manager
# and should be installed manually:
# - Boost Libraries >= 1.60.0
# - Umoci

# DOCS: END

mkdir -p /home/docker

echo "source scl_source enable devtoolset-11" >> /root/.bashrc
echo "source scl_source enable devtoolset-11" >> /root/.profile
echo "source scl_source enable devtoolset-11" >> /home/docker/.bashrc
echo "source scl_source enable devtoolset-11" >> /home/docker/.profile

echo 'export PATH=/usr/local/bin:${PATH}' >> /root/.bashrc
echo 'export PATH=/usr/local/bin:${PATH}' >> /root/.profile
echo 'export PATH=/usr/local/bin:${PATH}' >> /home/docker/.bashrc
echo 'export PATH=/usr/local/bin:${PATH}' >> /home/docker/.profile
