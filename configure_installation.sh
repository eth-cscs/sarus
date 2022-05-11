#!/bin/bash

# This script finalizes the configuration of a Sarus installation.
# The script is responsible for:
# - setting the proper permissions of the Sarus binary
# - creating the cached passwd database
# - creating the cached group database
# - ensuring root ownership of etc/sarus.schema.json
# - filling the missing parameters in etc/sarus.json

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

echo "Setting ownership of etc/sarus.schema.json"
chown root:root etc/sarus.schema.json
echo "Successfully set ownership of etc/sarus.schema.json"

echo "Configuring etc/sarus.json"

# create etc/sarus.json
if [ -e etc/sarus.json ]; then
    echo "Found existing configuration. Creating backup (etc/sarus.json.bak)."
    mv etc/sarus.json etc/sarus.json.bak
fi
cp etc/templates/sarus.json.in etc/sarus.json
exit_on_error "failed to create etc/sarus.json"

# configure PREFIX_DIR
sed -i etc/sarus.json -e "s|@INSTALL_PATH@|${prefix_dir}|g"
exit_on_error "failed to set INSTALL_PATH in etc/sarus.json"

# configure SKOPEO_PATH
skopeo_path=${prefix_dir}/bin/skopeo
if [ ! -e ${skopeo_path} ]; then
    skopeo_path=$(which skopeo)
    exit_on_error "failed to find skopeo binary. Is skopeo installed and available in \${PATH}?"
fi
echo "Found skopeo: ${skopeo_path}"
sed -i etc/sarus.json -e "s|@SKOPEO_PATH@|${skopeo_path}|g"
exit_on_error "failed to set SKOPEO_PATH in etc/sarus.json"

# configure UMOCI_PATH
umoci_path=${prefix_dir}/bin/umoci.amd64
if [ ! -e ${umoci_path} ]; then
    umoci_path=$(which umoci)
    exit_on_error "failed to find umoci binary. Is umoci installed and available in \${PATH}?"
fi
echo "Found umoci: ${umoci_path}"
sed -i etc/sarus.json -e "s|@UMOCI_PATH@|${umoci_path}|g"
exit_on_error "failed to set UMOCI_PATH in etc/sarus.json"

# configure MKSQUASHFS_PATH
mksquashfs_path=$(which mksquashfs)
exit_on_error "failed to find mksquashfs binary. Is squashfs-tools installed?"
echo "Found mksquashfs: ${mksquashfs_path}"
sed -i etc/sarus.json -e "s|@MKSQUASHFS_PATH@|${mksquashfs_path}|g"
exit_on_error "failed to set MKSQUASHFS_PATH in etc/sarus.json"

# configure INIT_PATH
init_path=${prefix_dir}/bin/tini-static-amd64
if [ ! -e ${init_path} ]; then
    # some distributions install tini at filesystem root
    if [ -e "/tini" ]; then
        init_path="/tini"
    elif [ -e "/tini-static" ]; then
        init_path="/tini-static"
    elif [ ! -z "$(which tini)" ]; then
        init_path=$(which tini)
    elif [ ! -z "$(which tini-static)" ]; then
        init_path=$(which tini-static)
    else
        false
        exit_on_error "failed to find tini or tini-static binary. Is tini installed and available in \${PATH}?"
    fi
fi
echo "Found init program: ${init_path}"
sed -i etc/sarus.json -e "s|@INIT_PATH@|${init_path}|g"
exit_on_error "failed to set INIT_PATH in etc/sarus.json"

# configure RUNC_PATH
runc_path=${prefix_dir}/bin/runc.amd64
if [ ! -e ${runc_path} ]; then
    runc_path=$(which runc)
    exit_on_error "failed to find runc binary. Is runc installed and available in \${PATH}?"
fi
echo "Found runc: ${runc_path}"
sed -i etc/sarus.json -e "s|@RUNC_PATH@|${runc_path}|g"
exit_on_error "failed to set RUNC_PATH in etc/sarus.json"

# configure repository dirs
sed -i etc/sarus.json -e "s|@LOCAL_REPO_DIR@|/home|g"
exit_on_error "failed to set LOCAL_REPO_DIR in etc/sarus.json"
sed -i etc/sarus.json -e "s|@CENTRAL_REPO_DIR@|/var/sarus/centralized_repository|g"
exit_on_error "failed to set CENTRAL_REPO_DIR in etc/sarus.json"

# configure sitemounts
sed -i etc/sarus.json -e 's|@SITE_MOUNTS@|{ \
                                            "type": "bind", \
                                            "source": "/home", \
                                            "destination": "/home", \
                                            "flags": {} \
                                          }|g'
exit_on_error "failed to set SITE_MOUNTS in etc/sarus.json"

# issue warning about tmpfs filesystem on Cray CLE <7
if [ -e /etc/opt/cray/release/cle-release ]; then
    cle_version=$(cat /etc/opt/cray/release/cle-release |head -n 1 |sed 's/RELEASE=\([0-9]\+\).*/\1/')
    if [ ${cle_version} -lt 7 ]; then
        echo "WARNING: on a Cray system with CLE <7 it is highly advised to set '\"ramFilesystemType\": \"tmpfs\"' in etc/sarus.json." \
             "Old CLE versions have a bug that makes the compute nodes crash when they attempt to unmount a tmpfs filesystem." >&2
    fi
fi

echo "Successfully configured etc/sarus.json."

# create OCI hooks in etc/hooks.d
if [ -e etc/hooks.d ]; then
    for file_json_in in $(cd etc/hooks.d && ls *.json.in); do
        file_json=$(basename ${file_json_in} .in)
        echo "Configuring etc/hooks.d/"${file_json}

        cp etc/hooks.d/${file_json_in} etc/hooks.d/${file_json}
        exit_on_error "failed to create etc/hooks.d/${file_json}"

        sed -i etc/hooks.d/${file_json} -e "s|@INSTALL_PATH@|${prefix_dir}|g"
        exit_on_error "failed to set INSTALL_PATH in etc/hooks.d/${file_json}"

        sed -i etc/hooks.d/${file_json} -e "s|@HOOK_BASE_DIR@|/home|g"
        exit_on_error "failed to set HOOK_BASE_DIR in etc/hooks.d/${file_json}"
    done
fi

echo "To execute sarus commands run first:"
echo "export PATH=${prefix_dir}/bin:\${PATH}"
echo "To persist that for future sessions, consider adding the previous line to your .bashrc or equivalent file"
