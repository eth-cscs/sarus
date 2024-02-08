#!/bin/bash

# This script generates the example JSON files to be imported into ReStructuredText doc pages for the
# hook configurations.
# The examples are generated by using ready-made input files available in the current directory or
# by instancing the templates in <sarus source root>/etc/templates/hooks.d> with suitable representative
# values

HOOKS_TEMPLATES_DIR=../../../etc/templates/hooks.d

cd $(dirname "${BASH_SOURCE[0]}")

# Qualify current directory files as JSON
for file_json_in in $(ls ./*.json.in); do
    file_json=$(basename ${file_json_in} .in)
    cp ${file_json_in} ./${file_json}
done

# Render templates with example values
for file_json_in in $(cd ${HOOKS_TEMPLATES_DIR} && ls *.json.in); do
    file_json=$(basename ${file_json_in} .in)
    cp ${HOOKS_TEMPLATES_DIR}/${file_json_in} ./${file_json}

    # BASE
    sed -i ${file_json} -e "s|@INSTALL_PATH@|/opt/sarus|g"
    sed -i ${file_json} -e "s|@HOOK_BASE_DIR@|/home|g"

    # GLIBC
    sed -i ${file_json} -e "s|@GLIBC_LIBS@|/lib64/libSegFault.so:/lib64/librt.so.1:/lib64/libnss_dns.so.2:/lib64/libanl.so.1:/lib64/libresolv.so.2:/lib64/libnsl.so.1:/lib64/libBrokenLocale.so.1:/lib64/ld-linux-x86-64.so.2:/lib64/libnss_hesiod.so.2:/lib64/libutil.so.1:/lib64/libnss_files.so.2:/lib64/libnss_compat.so.2:/lib64/libnss_db.so.2:/lib64/libm.so.6:/lib64/libcrypt.so.1:/lib64/libc.so.6:/lib64/libpthread.so.0:/lib64/libdl.so.2:/lib64/libmvec.so.1:/lib64/libc.so.6:/lib64/libthread_db.so.1|g"

    # NVIDIA
    sed -i ${file_json} -e "s|@NVIDIA_TOOLKIT_PATH@|/opt/sarus/bin|g"
    sed -i ${file_json} -e "s|@LIBNVIDIA_CONTAINER_PATH@|/usr/local/libnvidia-container_1.14.5|g"

    # MPI
    sed -i ${file_json} -e "s|@MPI_LIBS@|/usr/lib64/mvapich2-2.2/lib/libmpi.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpicxx.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpifort.so.12.0.5|g"
    sed -i ${file_json} -e "s|@MPI_DEPENDENCY_LIBS@||g"
    sed -i ${file_json} -e "s|@MPI_BIND_MOUNTS@||g"

    # MOUNT
    sed -i ${file_json} -e "s|@MOUNT_ARGS@|\"--mount=type=bind,src=/usr/lib/libexample.so.1,dst=/usr/local/lib/libexample.so.1\",\n            \"--mount=type=bind,src=/etc/configs,dst=/etc/host_configs,readonly\",\n            \"--device=/dev/example:rw\"|g"
done
