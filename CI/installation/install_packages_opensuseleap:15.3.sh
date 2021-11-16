#!/bin/bash

set -ex

# Install packages
sudo zypper install -y gcc-c++ glibc-static wget which git gzip bzip2 \
    make autoconf automake libtool squashfs libcap-devel cmake \
    zlib-devel zlib-devel-static libopenssl-devel libexpat-devel \
    libarchive-devel runc tini-static \
    libboost_atomic1_75_0-devel \
    libboost_chrono1_75_0-devel \
    libboost_filesystem1_75_0-devel \
    libboost_random1_75_0-devel \
    libboost_regex1_75_0-devel \
    libboost_system1_75_0-devel \
    libboost_thread1_75_0-devel \
    libboost_program_options1_75_0-devel \
    libboost_date_time1_75_0-devel \
    python3 python3-pip python3-setuptools
sudo zypper clean --all

# DOCS: END

