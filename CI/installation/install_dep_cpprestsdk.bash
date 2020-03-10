#!/bin/bash
set -ex
pwd_bak=$PWD

# Install cpprestsdk
cd /tmp && \
    ([ -e /usr/include/xlocale.h ] || ln -s /usr/include/locale.h /usr/include/xlocale.h) && \
    mkdir -p cpprestsdk/v2.10.0 && cd cpprestsdk/v2.10.0 && \
    wget https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz && \
    tar xf v2.10.0.tar.gz && \
    mv cpprestsdk-2.10.0 src && cd src/Release && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=0 -DWERROR=FALSE .. && \
    sed -i /tmp/cpprestsdk/v2.10.0/src/Release/include/cpprest/asyncrt_utils.h -e '1s/^/#include <sys\/time.h>\n/' || true && \
    make -j$(nproc) && \
    sudo make install && \
    cd ${pwd_bak} && \
    rm -rf /tmp/cpprestsdk
