#!/bin/bash

fail_on_error() {
    local last_command_exit_code=$?
    local message=$1
    if [ ${last_command_exit_code} -ne 0 ]; then
        echo "${message}"
        exit 1
    fi
}

set -ex

# On CentOS 7, enable devtoolset to use GCC 4.9.2
if [ $(grep centos:7 /etc/os-release) ]; then
    source /opt/rh/devtoolset-3/enable
fi

sarus_sources_external=/sarus-source
sarus_sources_root=${HOME}/sarus-source
cp -r ${sarus_sources_external} ${sarus_sources_root}

# Chdir to Sarus source directory
cd ${sarus_sources_root}

# Create build dir
mkdir build
cd build

# Configure and build
export install_prefix=/opt/sarus  # this will be used as installation path

cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=${install_prefix} \
      ..

make -j$(nproc)

# Install Sarus
sudo make install

# Create OCI bundle dir
sudo mkdir -p ${install_prefix}/var/OCIBundleDir

# Finalize installation
sudo ${install_prefix}/configure_installation.sh
export PATH=${install_prefix}/bin:${PATH}

# Check installation
sarus --version
fail_on_error "Failed to call Sarus"

sarus pull alpine
fail_on_error "Failed to pull image"

sarus images
fail_on_error "Failed to list images"

sarus run alpine cat /etc/os-release > sarus.out
if [ "$(head -n 1 sarus.out)" != "NAME=\"Alpine Linux\"" ]; then
    echo "Failed to run container"
    exit 1
fi
