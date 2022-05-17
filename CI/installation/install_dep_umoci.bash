#!/bin/bash
set -ex
pwd_bak=$PWD

# Install umoci
cd /tmp && \
    wget -O umoci https://github.com/opencontainers/umoci/releases/download/v0.4.7/umoci.amd64 && \
    chmod 755 umoci && \
    sudo mv umoci /usr/local/bin/ && \
    sudo chown root:root /usr/local/bin/umoci && \
    cd ${pwd_bak}
