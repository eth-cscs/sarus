#!/bin/bash

set -ex

# Install packages
sudo dnf install -y gcc g++ glibc-static git make cmake autoconf \
    libtool squashfs-tools zlib-devel zlib-static openssl-devel \
    boost-devel cpprest-devel expat-devel libarchive-devel \
    runc tini-static wget which procps \
    python3 python3-pip python3-setuptools
sudo dnf clean all
sudo rm -rf /var/cache/dnf

# DOCS: END

