#!/bin/bash

set -ex

# Install runc
cd /usr/local/bin && \
    wget -O runc.amd64 https://github.com/opencontainers/runc/releases/download/v1.0.0-rc10/runc.amd64 && \
    chmod 755 runc.amd64 && \
    chown root:root runc.amd64