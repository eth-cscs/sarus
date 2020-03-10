#!/bin/bash
set -ex
pwd_bak=$PWD

# Install libarchive
cd /tmp && \
    mkdir -p libarchive/3.4.1 && \
    cd libarchive/3.4.1 && \
    wget https://github.com/libarchive/libarchive/releases/download/v3.4.1/libarchive-3.4.1.tar.gz && \
    tar xvf libarchive-3.4.1.tar.gz && \
    mv libarchive-3.4.1 src && \
    mkdir src/build-cmake && cd src/build-cmake && \
    cmake .. && \
    make -j$(nproc) && \
    sudo make install && \
    cd ${pwd_bak} && \
    rm -rf /tmp/libarchive