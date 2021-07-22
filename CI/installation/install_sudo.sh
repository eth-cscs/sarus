#!/bin/bash
set -ex

# OS-specific installation of "sudo"
os=$1
if   [ "$os" == "ubuntu:18.04" ]; then
    apt-get update && apt-get install -y sudo
elif [ "$os" == "ubuntu:20.04" ]; then
    apt-get update && apt-get install -y sudo
elif [ "$os" == "debian:10" ]; then
    apt-get update && apt-get install -y sudo
elif [ "$os" == "centos:7" ]; then
    yum install -y sudo
else
    echo "Unsupported OS specified"
    exit 1
fi

echo 'root ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
