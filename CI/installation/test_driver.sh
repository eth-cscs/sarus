#!/bin/bash

set -ex

os=$1

# OS-specific installation of "sudo"
if   [ "$os" == "ubuntu:18.04" ]; then
    apt-get update && apt-get install -y sudo
elif [ "$os" == "debian:10" ]; then
    apt-get update && apt-get install -y sudo
elif [ "$os" == "centos:7" ]; then
    yum install -y sudo
else
    echo "Unsupported OS specified"
    exit 1
fi

# Add normal user
useradd -m docker && echo "docker:docker" | chpasswd
echo "docker ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

su --login docker /sarus-source/CI/installation/install_packages_${os}.sh
su --login docker /sarus-source/CI/installation/install_dependencies.sh
su --login docker /sarus-source/CI/installation/install_sarus_from_source_and_test.sh
