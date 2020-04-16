#!/bin/bash
set -ex

# Install packages
sudo apt-get update --fix-missing
sudo apt-get install -y --no-install-recommends build-essential
sudo apt-get install -y --no-install-recommends \
   kmod rsync curl gdb git vim autoconf automake libtool \
   squashfs-tools libcap-dev cmake wget zlib1g-dev libssl-dev \
   libexpat1-dev ca-certificates \
   python3 python3-pip python3-setuptools
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
sudo update-ca-certificates

# DOCS: END

mkdir -p /home/docker
echo 'export PATH=${PATH}:/usr/sbin:/sbin' >> /home/docker/.bashrc
echo 'export PATH=${PATH}:/usr/sbin:/sbin' >> /home/docker/.profile
