#!/bin/bash

# This script finalizes the configuration of a Sarus installation made from the Sarus standalone archive.
# The script is responsible for setting the proper permissions of the Sarus binary,
# as well as filling the missing parameters in the etc/sarus.json.

function exit_on_error() {
    local last_command_exit_code=$?
    local message=$1
    if [ ${last_command_exit_code} -ne 0 ]; then
        echo "ERROR: "${message}
        exit 1
    fi
}

prefix_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd ${prefix_dir}

echo "Setting Sarus as SUID root"
sudo chown root:root bin/sarus
exit_on_error "failed to chown bin/sarus"
sudo chmod +s bin/sarus
exit_on_error "failed to chmod bin/sarus"
echo "Successfully set Sarus as SUID root"

echo "Creating cached passwd database"
getent passwd >files_to_copy_in_container_etc/passwd
echo "Successfully created cached passwd database"

echo "Configuring etc/sarus.json"

# create etc/sarus.json
if [ -e etc/sarus.json ]; then
    echo "Found existing configuration. Creating backup (/etc/sarus.json.bak)."
    mv etc/sarus.json etc/sarus.json.bak
fi
cp etc/sarus.json.in etc/sarus.json
exit_on_error "failed to create etc/sarus.json"

# configure PREFIX_DIR
sed -i etc/sarus.json -e "s|@CMAKE_INSTALL_PREFIX@|${prefix_dir}|g"
exit_on_error "failed to set CMAKE_INSTALL_PREFIX in etc/sarus.json"

# configure RUNC_PATH
runc_path=${prefix_dir}/bin/runc.amd64
sed -i etc/sarus.json -e "s|@RUNC_PATH@|${runc_path}|g"
exit_on_error "failed to set RUNC_PATH in etc/sarus.json"

# configure MKSQUASHFS_PATH
mksquashfs_path=$(which mksquashfs)
exit_on_error "failed to find mksquashfs binary. Is squashfs-tools installed?"
sed -i etc/sarus.json -e "s|@MKSQUASHFS_PATH@|${mksquashfs_path}|g"
exit_on_error "failed to set MKSQUASHFS_PATH in etc/sarus.json"

echo "Successfully configured etc/sarus.json"
