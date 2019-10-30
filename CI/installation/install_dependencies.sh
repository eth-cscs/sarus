#!/bin/bash

set -ex

# On CentOS 7, enable devtoolset to use GCC 4.9.2
if [ $(grep centos:7 /etc/os-release) ]; then
    source /opt/rh/devtoolset-3/enable
fi

# Dependencies from source --------------

# Create a dedicated working directory for clarity
export sarus_deps_workdir=${HOME}/sarus-deps
mkdir -p ${sarus_deps_workdir}
cd ${sarus_deps_workdir}

# Install libarchive
mkdir -p libarchive/3.3.1 && cd libarchive/3.3.1
wget https://github.com/libarchive/libarchive/archive/v3.3.1.tar.gz
tar xvzf v3.3.1.tar.gz
mv libarchive-3.3.1 src
mkdir src/build-cmake && cd src/build-cmake
cmake ..
make -j$(nproc)
sudo make install
cd ${sarus_deps_workdir}
rm -rf libarchive/  # cleanup sources and build artifacts

# Install boost
mkdir -p boost/1_65_0 && cd boost/1_65_0
wget https://downloads.sourceforge.net/project/boost/boost/1.65.0/boost_1_65_0.tar.bz2
tar xf boost_1_65_0.tar.bz2
mv boost_1_65_0 src && cd src
./bootstrap.sh
sudo ./b2 -j$(nproc) \
    --with-atomic \
    --with-chrono \
    --with-filesystem \
    --with-random \
    --with-regex \
    --with-system \
    --with-thread \
    --with-program_options \
    --with-date_time \
    install
cd ${sarus_deps_workdir}
sudo rm -r boost/  # cleanup sources and build artifacts

# Install cpprestsdk
mkdir -p cpprestsdk/v2.10.0 && cd cpprestsdk/v2.10.0
wget https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz
tar xf v2.10.0.tar.gz
mv cpprestsdk-2.10.0 src && cd src/Release
mkdir build && cd build
cmake -DWERROR=FALSE ..
make -j$(nproc)
sudo make install
cd ${sarus_deps_workdir}
rm -rf cpprestsdk/  # cleanup sources and build artifacts

# Install RapidJSON
wget -O rapidjson.tar.gz https://github.com/Tencent/rapidjson/archive/663f076c7b44ce96526d1acfda3fa46971c8af31.tar.gz
tar xvzf rapidjson.tar.gz && cd rapidjson-663f076c7b44ce96526d1acfda3fa46971c8af31
sudo cp -r include/rapidjson /usr/local/include/rapidjson
cd ${sarus_deps_workdir}
rm -rf rapidjson.tar.gz rapidjson-663f076c7b44ce96526d1acfda3fa46971c8af31  #cleanup sources


# Pre-built dependencies --------------

# Install runc
wget -O runc https://github.com/opencontainers/runc/releases/download/v1.0.0-rc9/runc.amd64
chmod 755 runc                           # make it executable
sudo mv runc /usr/local/bin/             # add it to PATH
sudo chown root:root /usr/local/bin/runc # set root ownership for security

# Install tini
wget -O tini https://github.com/krallin/tini/releases/download/v0.18.0/tini-static-amd64
chmod 755 tini                           # make it executable
sudo mv tini /usr/local/bin/             # add it to PATH
sudo chown root:root /usr/local/bin/tini # set root ownership for security
