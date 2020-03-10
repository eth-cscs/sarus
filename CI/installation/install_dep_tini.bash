#!/bin/bash
set -ex
pwd_bak=$PWD

# Install tini
cd /tmp && \
    wget -O tini-static-amd64 https://github.com/krallin/tini/releases/download/v0.18.0/tini-static-amd64 && \
    chmod 755 tini-static-amd64 && \
    sudo mv tini-static-amd64 /usr/local/bin/ && \
    sudo chown root:root /usr/local/bin/tini-static-amd64 && \
    cd ${pwd_bak}