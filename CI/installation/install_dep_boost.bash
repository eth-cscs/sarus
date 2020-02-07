#!/bin/bash

set -ex

# Install boost
cd /opt && \
    mkdir -p boost/1_65_0 && cd boost/1_65_0 && \
    wget https://downloads.sourceforge.net/project/boost/boost/1.65.0/boost_1_65_0.tar.bz2 && \
    tar xf boost_1_65_0.tar.bz2 && \
    mv boost_1_65_0 src && cd src && \
    ./bootstrap.sh && \
    ./b2 -j$(nproc) \
        --with-atomic \
        --with-chrono \
        --with-filesystem \
        --with-random \
        --with-regex \
        --with-system \
        --with-thread \
        --with-program_options \
        --with-date_time \
        install && \
    cd .. && \
    rm -r src
