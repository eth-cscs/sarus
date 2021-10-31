#!/bin/bash
set -ex
pwd_bak=$PWD

# Install boost
cd /tmp && \
    mkdir -p boost/1_77_0 && cd boost/1_77_0 && \
    wget https://downloads.sourceforge.net/project/boost/boost/1.77.0/boost_1_77_0.tar.bz2 && \
    tar xf boost_1_77_0.tar.bz2 && \
    mv boost_1_77_0 src && cd src && \
    ./bootstrap.sh && \
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
        install && \
    cd ${pwd_bak} && \
    rm -rf /tmp/boost
