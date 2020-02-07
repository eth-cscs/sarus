#!/bin/bash

set -ex

# Install libarchive
cd /opt/ && \
    mkdir -p libarchive/3.4.1 && \
    cd libarchive/3.4.1 && \
    wget https://github.com/libarchive/libarchive/releases/download/v3.4.1/libarchive-3.4.1.tar.gz && \
    tar xvf libarchive-3.4.1.tar.gz && \
    mv libarchive-3.4.1 src && \
    mkdir src/build-cmake && cd src/build-cmake && \
    cmake -DCMAKE_INSTALL_PREFIX:PATH=../.. .. && \
    make -j$(nproc) && \
    make install && \
    cd ../.. && \
    rm -r src