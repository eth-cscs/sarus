#!/bin/bash
set -ex
pwd_bak=$PWD

# Install RapidJSON
cd /tmp && \
    mkdir -p rapidjson && cd rapidjson && \
    wget -O rapidjson-master.tar.gz https://github.com/Tencent/rapidjson/archive/663f076c7b44ce96526d1acfda3fa46971c8af31.tar.gz && \
    tar xvzf rapidjson-master.tar.gz && \
    cd rapidjson-663f076c7b44ce96526d1acfda3fa46971c8af31 && \
    sudo mkdir -p /usr/local/include/rapidjson && sudo cp -r include/rapidjson /usr/local/include/rapidjson && \
    cd ${pwd_bak} && \
    rm -rf /tmp/rapidjson