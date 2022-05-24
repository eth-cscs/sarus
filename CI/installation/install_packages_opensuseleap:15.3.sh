#!/bin/bash

set -ex

# Install packages
sudo zypper install -y gcc-c++ glibc-static wget which git gzip bzip2 tar \
    make autoconf automake squashfs cmake zlib-devel zlib-devel-static \
    runc tini-static skopeo umoci \
    libboost_filesystem1_75_0-devel \
    libboost_regex1_75_0-devel \
    libboost_program_options1_75_0-devel \
    python3 python3-pip python3-setuptools
sudo zypper clean --all

# DOCS: END

