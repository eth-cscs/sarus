#!/bin/bash

set -ex

# Install cpprestsdk
cd /opt && \
    ln -s /usr/include/locale.h /usr/include/xlocale.h && \
    mkdir -p cpprestsdk/v2.10.0 && cd cpprestsdk/v2.10.0 && \
    wget https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz && \
    tar xf v2.10.0.tar.gz && \
    mv cpprestsdk-2.10.0 src && cd src/Release && \
    mkdir build && cd build && \
    cmake cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=0 -DWERROR=FALSE -DCMAKE_PREFIX_PATH=../../../../../boost/1_65_0 -DCMAKE_INSTALL_PREFIX=../../.. .. && \
    sed -i /opt/cpprestsdk/v2.10.0/src/Release/include/cpprest/asyncrt_utils.h -e '1s/^/#include <sys\/time.h>\n/' && \
    make -j$(nproc) && \
    make install && \
    cd ../../.. && \
    rm -rf src