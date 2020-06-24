#!/bin/bash
set -ex

# Install packages
sudo yum install -y epel-release
sudo yum install -y centos-release-scl-rh
sudo yum install -y devtoolset-8-gcc-c++ glibc-static sudo curl wget rsync which \
    make bzip2 autoconf automake libtool squashfs-tools libcap-devel cmake3 \
    zlib-devel zlib-static openssl-devel expat-devel git \
    python3 python3-pip python3-setuptools
sudo yum clean all
sudo rm -rf /var/cache/yum

# Create symlink to cmake3 to uniform commands with other distributions
sudo ln -s /usr/bin/cmake3 /usr/bin/cmake
sudo ln -s /usr/bin/ctest3 /usr/bin/ctest

# Enable devtoolset-3 to use GCC 4.9.2
source /opt/rh/devtoolset-8/enable

# DOCS: END

mkdir -p /home/docker

echo "source scl_source enable devtoolset-8" >> /root/.bashrc
echo "source scl_source enable devtoolset-8" >> /root/.profile
echo "source scl_source enable devtoolset-8" >> /home/docker/.bashrc
echo "source scl_source enable devtoolset-8" >> /home/docker/.profile

echo 'export PATH=/usr/local/bin:${PATH}' >> /root/.bashrc
echo 'export PATH=/usr/local/bin:${PATH}' >> /root/.profile
echo 'export PATH=/usr/local/bin:${PATH}' >> /home/docker/.bashrc
echo 'export PATH=/usr/local/bin:${PATH}' >> /home/docker/.profile
