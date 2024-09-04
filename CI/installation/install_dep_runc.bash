#!/bin/bash
set -ex
pwd_bak=$PWD

# Install runc
cd /tmp && \
    wget -O runc.amd64 https://github.com/opencontainers/runc/releases/download/v1.1.14/runc.amd64 && \
    chmod 755 runc.amd64 && \
    sudo mv runc.amd64 /usr/local/bin/ && \
    sudo chown root:root /usr/local/bin/runc.amd64 && \
    cd ${pwd_bak}
