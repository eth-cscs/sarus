#!/bin/bash

# This script finalizes the configuration of a Sarus installation made from the Sarus standalone archive.
# The script is responsible for filling the missing parameters in the etc/sarus.json.

function exit_on_error() {
    local last_command_exit_code=$?
    local message=$1
    if [ ${last_command_exit_code} -ne 0 ]; then
        echo "ERROR: "${message}
        exit 1
    fi
}

echo "Configuring etc/sarus.json"

prefix_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd ${prefix_dir}

# create etc/sarus.json
if [ -e etc/sarus.json ]; then
    echo "Found existing configuration. Creating backup (/etc/sarus.json.bak)."
    mv etc/sarus.json etc/sarus.json.bak
fi
cp etc/sarus.json.in etc/sarus.json
exit_on_error "failed to create etc/sarus.json"

# configure PREFIX_DIR
sed -i etc/sarus.json -e "s|@PREFIX_DIR@|${prefix_dir}|g"
exit_on_error "failed to set PREFIX_DIR in etc/sarus.json"

# configure MKSQUASHFS_PATH
mksquashfs_path=$(which mksquashfs)
exit_on_error "failed to find mksquashfs binary. Is squashfs-tools installed?"
sed -i etc/sarus.json -e "s|@MKSQUASHFS_PATH@|${mksquashfs_path}|g"
exit_on_error "failed to set MKSQUASHFS_PATH in etc/sarus.json"

echo "Successfully configured etc/sarus.json"
