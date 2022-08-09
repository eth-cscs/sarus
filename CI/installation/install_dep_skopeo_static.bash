#!/bin/bash
set -ex
pwd_bak=$PWD

# Build and install a static skopeo
# WARNING! Static builds are not supported by Skopeo maintainers
# and are not suitable for production uses!
cd /tmp && \
    git clone https://github.com/containers/skopeo.git && \
    cd skopeo/ && \
    git checkout v1.7.0 && \
    export CGO_ENABLED=0 && \
    make BUILDTAGS=containers_image_openpgp GO_DYN_FLAGS= bin/skopeo && \
    chmod 755 bin/skopeo && \
    sudo mv bin/skopeo /usr/local/bin/ && \
    sudo chown root:root /usr/local/bin/skopeo && \
    cd ${pwd_bak}
