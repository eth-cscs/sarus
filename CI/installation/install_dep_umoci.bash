#!/bin/bash
set -ex
pwd_bak=$PWD

# Install umoci
cd /tmp && \
    wget -O umoci.amd64 https://github.com/opencontainers/umoci/releases/download/v0.4.7/umoci.amd64 && \
    chmod 755 umoci.amd64 && \
    sudo mv umoci.amd64 /usr/local/bin/ && \
    sudo chown root:root /usr/local/bin/umoci.amd64 && \
    cd ${pwd_bak}
