#!/bin/bash

# This script finalizes the configuration of a Sarus installation made from the Sarus standalone archive.
# The script is responsible for:
# - setting the proper permissions of the Sarus binary
# - creating the cached passwd database
# - creating the cached group database
# - filling the missing parameters in the etc/sarus.json.

function exit_on_error() {
    local last_command_exit_code=$?
    local message=$1
    if [ ${last_command_exit_code} -ne 0 ]; then
        echo "ERROR: "${message}
        exit 1
    fi
}

if [ "$EUID" -ne 0 ]; then
    echo "ERROR: you need to run this script as root" >&2
    exit 1
fi

prefix_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd ${prefix_dir}

echo "Setting Sarus as SUID root"
chown root:root bin/sarus
exit_on_error "failed to chown bin/sarus"
chmod +s bin/sarus
exit_on_error "failed to chmod bin/sarus"
echo "Successfully set Sarus as SUID root"

echo "Creating cached passwd database"
getent passwd > etc/passwd
echo "Successfully created cached passwd database"

echo "Creating cached group database"
getent group > etc/group
echo "Successfully created cached group database"

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

# issue warning about tmpfs filesystem on Cray CLE <7
if [ -e /etc/opt/cray/release/cle-release ]; then
    cle_version=$(cat /etc/opt/cray/release/cle-release |head -n 1 |sed 's/RELEASE=\([0-9]\+\).*/\1/')
    if [ ${cle_version} -lt 7 ]; then
        echo "WARNING: on a Cray system with CLE <7 it is highly advised to set '\"ramFilesystemType\": \"tmpfs\"' in etc/sarus.json." \
             "Old CLE versions have a bug that make the compute nodes crash when they attempt to unmount a tmpfs filesystem." >&2
    fi
fi

echo "Successfully configured etc/sarus.json"