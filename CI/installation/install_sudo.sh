#!/bin/bash
set -ex

# OS-specific installation of "sudo"
os=$1
if [[ "$os" == ubuntu:* ]]; then
    apt-get update && apt-get install -y sudo
elif [[ "$os" == debian:* ]]; then
    apt-get update && apt-get install -y sudo
elif [[ "$os" == rocky:* ]]; then
    yum install -y sudo
elif [[ "$os" == fedora:* ]]; then
    dnf install -y sudo
elif [[ "$os" == opensuseleap:* ]]; then
    zypper install -y sudo
else
    echo "Unsupported OS specified"
    exit 1
fi

echo 'root ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
