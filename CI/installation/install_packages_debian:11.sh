#!/bin/bash
set -ex

# Install packages
sudo apt-get update --fix-missing
sudo apt-get install -y --no-install-recommends build-essential
sudo DEBIAN_FRONTEND=noninteractive \
   apt-get install -y --no-install-recommends kmod curl git \
   autoconf automake gnupg squashfs-tools cmake wget \
   zlib1g-dev runc tini skopeo umoci \
   libboost-dev \
   libboost-program-options-dev \
   libboost-filesystem-dev \
   libboost-regex-dev \
   python3 python3-pip python3-setuptools
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*

# DOCS: END

mkdir -p /home/docker
echo 'export PATH=${PATH}:/usr/sbin:/sbin' >> /home/docker/.bashrc
echo 'export PATH=${PATH}:/usr/sbin:/sbin' >> /home/docker/.profile
