#!/bin/bash

sarus_src_dir=/sarus-source

# Setup Spack bash integration
. ${SPACK_ROOT}/share/spack/setup-env.sh

# Import Spack package for Sarus
cp -r $sarus_src_dir/spack/packages/sarus ${SPACK_LOCAL_REPO}/packages/

# Create mirror with local sources
export SPACK_LOCAL_MIRROR=${SPACK_ROOT}/mirror/
export SARUS_MIRROR_DIR=${SPACK_LOCAL_MIRROR}/sarus/
mkdir -p ${SARUS_MIRROR_DIR}
cp -r $sarus_src_dir ${SARUS_MIRROR_DIR}/spack-src
cd ${SARUS_MIRROR_DIR}
tar -czf sarus-develop.tar.gz spack-src
rm -rf spack-src
cd
spack mirror add local_filesystem file://${SPACK_LOCAL_MIRROR}

# Install sarus
spack install --verbose sarus@develop

# Check installation
spack load sarus
sarus pull alpine
if [ "$?" != "0" ]; then
    exit 1
fi

sarus run alpine cat /etc/os-release > sarus.out
if [ "$(head -n 1 sarus.out)" != "NAME=\"Alpine Linux\"" ]; then
    exit 1
fi


