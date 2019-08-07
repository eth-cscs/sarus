FROM ubuntu:18.04

LABEL version="1.0.0"

# Install requirements to build sarus
# Note: run apt-get install in two phases to avoid errors
# (E: Failed to fetch http://archive.ubuntu.com/ubuntu/pool/main/g/gcc-7/libgcc-7-dev_7.3.0-16ubuntu3_amd64.deb 
# Undetermined Error [IP: 91.189.88.149 80])
RUN apt-get update --fix-missing \
    && apt-get install -y --no-install-recommends build-essential \
    && apt-get install -y --no-install-recommends \
          kmod sudo rsync curl gdb git vim \
          autoconf automake libtool valgrind xfsprogs \
          squashfs-tools libcap-dev cmake wget python python-pip python3-venv \
          zlib1g-dev libboost-all-dev libssl-dev munge slurm-wlm golang \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# install libarchive
RUN wget -O libarchive-v3.3.1.tar.gz https://github.com/libarchive/libarchive/archive/v3.3.1.tar.gz \
    && tar xvzf libarchive-v3.3.1.tar.gz && cd libarchive-3.3.1 \
    && cmake . \
    && make -j8 \
    && make install \
    && cd .. \
    && rm -rf libarchive-v3.3.1.tar.gz libarchive-3.3.1

# install cpprestsdk
RUN wget -O cpprestsdk-v2.10.0.tar.gz https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz \
    && tar xvzf cpprestsdk-v2.10.0.tar.gz && cd cpprestsdk-2.10.0 \
    && mkdir build.debug && cd build.debug \
    && cmake ../Release -DCMAKE_BUILD_TYPE=Debug \
    && make -j8 \
    && make install \
    && cd ../.. \
    && rm -rf cpprestsdk-v2.10.0.tar.gz cpprestsdk-2.10.0

# install RapidJSON
# We download from the master branch because the latest tagged release (v1.1.0 at the time of writing)
# does not feature the SchemaValidatingReader::GetError() function
RUN wget -O rapidjson-master.tar.gz https://github.com/Tencent/rapidjson/archive/master.tar.gz \
    && tar xvzf rapidjson-master.tar.gz && cd rapidjson-master \
    && cp -r include/rapidjson /usr/local/include/rapidjson \
    && cd ../.. \
    && rm -rf rapidjson-master.tar.gz rapidjson-master

# install runc
RUN wget https://github.com/opencontainers/runc/releases/download/v1.0.0-rc8/runc.amd64 \
    && mv runc.amd64 /sbin/runc.amd64 \
    && chmod 755 /sbin/runc.amd64

RUN pip install setuptools
RUN pip install nose gcovr pexpect
RUN pip install sphinx sphinx-rtd-theme

RUN useradd -m docker && echo "docker:docker" | chpasswd
RUN echo "docker ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER docker
WORKDIR /home/docker
CMD ["/bin/bash"]
