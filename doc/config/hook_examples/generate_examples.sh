#!/bin/bash

# This script instances the hook configuration templates in <sarus source root>/etc/templates/hooks.d
# with plausible values in order to generate the example JSON files to import into ReStructuredText doc
# pages for the hook configurations

HOOKS_TEMPLATES_DIR=../../../etc/templates/hooks.d

cd $(dirname "${BASH_SOURCE[0]}")

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
    sed -i ${file_json} -e "s|@LIBNVIDIA_CONTAINER_PATH@|/usr/local/libnvidia-container_1.7.0|g"

    # MPI
    sed -i ${file_json} -e "s|@MPI_LIBS@|/usr/lib64/mvapich2-2.2/lib/libmpi.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpicxx.so.12.0.5:/usr/lib64/mvapich2-2.2/lib/libmpifort.so.12.0.5|g"
    sed -i ${file_json} -e "s|@MPI_DEPENDENCY_LIBS@||g"
    sed -i ${file_json} -e "s|@MPI_BIND_MOUNTS@||g"
done
