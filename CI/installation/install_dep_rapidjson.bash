#!/bin/bash
set -ex
pwd_bak=$PWD

# Install RapidJSON
cd /tmp && \
    mkdir -p rapidjson && cd rapidjson && \
    wget -O rapidjson-master.tar.gz https://github.com/Tencent/rapidjson/archive/00dbcf2c6e03c47d6c399338b6de060c71356464.tar.gz && \
    tar xvzf rapidjson-master.tar.gz && \
    cd rapidjson-00dbcf2c6e03c47d6c399338b6de060c71356464 && \
    sudo mkdir -p /usr/local/include/rapidjson && sudo cp -r include/rapidjson/* /usr/local/include/rapidjson/ && \
    cd ${pwd_bak} && \
    rm -rf /tmp/rapidjson
