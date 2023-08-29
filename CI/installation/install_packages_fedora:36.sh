#!/bin/bash

set -ex

# Install packages
sudo dnf install -y gcc g++ glibc-static libstdc++-static git make cmake autoconf \
    diffutils findutils wget which procps squashfs-tools zlib-devel zlib-static \
    boost-devel skopeo tini-static \
    python3 python3-pip python3-setuptools
sudo dnf clean all
sudo rm -rf /var/cache/dnf

# The following dependencies are not provided via the system's package manager
# and should be installed manually:
# - Runc version 1.1.9
# - Umoci

# DOCS: END

