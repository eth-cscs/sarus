#!/bin/bash

set -ex

# Install RapidJSON
cd /opt && \
    mkdir rapidjson && cd rapidjson && \
    wget -O rapidjson-master.tar.gz https://github.com/Tencent/rapidjson/archive/663f076c7b44ce96526d1acfda3fa46971c8af31.tar.gz && \
    tar xvzf rapidjson-master.tar.gz && cd rapidjson-663f076c7b44ce96526d1acfda3fa46971c8af31 && \
    cp -r include/rapidjson /usr/local/include/rapidjson && \
    rm -rf rapidjson.tar.gz rapidjson-663f076c7b44ce96526d1acfda3fa46971c8af31