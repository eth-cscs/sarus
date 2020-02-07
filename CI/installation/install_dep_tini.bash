#!/bin/bash

set -ex

# Install tini
cd /usr/local/bin && \
    wget -O tini-static-amd64 https://github.com/krallin/tini/releases/download/v0.18.0/tini-static-amd64 && \
    chmod 755 tini-static-amd64 && \
    chown root:root tini-static-amd64