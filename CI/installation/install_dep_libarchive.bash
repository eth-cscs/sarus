#!/bin/bash
set -ex
pwd_bak=$PWD

# Install libarchive
cd /tmp && \
    mkdir -p libarchive/3.5.2 && \
    cd libarchive/3.5.2 && \
    wget https://github.com/libarchive/libarchive/releases/download/v3.5.2/libarchive-3.5.2.tar.gz && \
    tar xvf libarchive-3.5.2.tar.gz && \
    mv libarchive-3.5.2 src && \
    mkdir src/build-cmake && cd src/build-cmake && \
    cmake .. && \
    make -j$(nproc) && \
    sudo make install && \
    cd ${pwd_bak} && \
    rm -rf /tmp/libarchive
