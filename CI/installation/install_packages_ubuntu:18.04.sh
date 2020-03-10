#!/bin/bash
set -ex

# Install packages
sudo apt-get update --fix-missing
sudo apt-get install -y --no-install-recommends build-essential
sudo apt-get install -y --no-install-recommends \
   kmod sudo rsync curl gdb git vim autoconf automake libtool \
   squashfs-tools libcap-dev cmake wget zlib1g-dev libssl-dev \
   libexpat1-dev ca-certificates
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
sudo update-ca-certificates
