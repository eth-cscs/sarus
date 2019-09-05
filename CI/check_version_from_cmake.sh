#!/bin/bash

log() {
    local message=$1
    echo "[ LOG ]  $message"
}

cleanup() {
    rm -rf /home/docker/sarus-git
    rm -rf /home/docker/sarus-static
}

cleanup_and_exit_if_last_command_failed() {
    local last_command_exit_code=$?
    if [ $last_command_exit_code -ne 0 ]; then
        log "command failed"
        cleanup
        exit 1
    fi
}

sarus_src_dir=/sarus-source

check_static_snapshot() {
    log "Running CMake from static snapshot"
    mkdir -p /home/docker/sarus-static/build
    cp -r $sarus_src_dir/* /home/docker/sarus-static
    cd /home/docker/sarus-static/build
    cmake   -DCMAKE_PREFIX_PATH="/opt/boost/1_65_0;/opt/cpprestsdk/v2.10.0;/opt/libarchive/3.3.1;/opt/rapidjson/rapidjson-master" \
            -Dcpprestsdk_INCLUDE_DIR=/opt/cpprestsdk/v2.10.0/include \
            -DBUILD_STATIC=TRUE \
            .. \
            > cmake_stdout.txt
    cleanup_and_exit_if_last_command_failed
    log "    Config successful, checking version string"
    version_from_cmake=$(cat cmake_stdout.txt | grep "Sarus version" | awk -F ": " '{print $2}')
    version_from_file=$(cat ../VERSION)
    log "    Version from CMake: ${version_from_cmake}"
    log "    Version from file : ${version_from_file}" 
    [ "$version_from_file" == "$version_from_cmake" ]
    cleanup_and_exit_if_last_command_failed
    log "    Check successful"
}

check_git_repo() {
    log "Running CMake from git repository"
    cp -rT $sarus_src_dir /home/docker/sarus-git
    mkdir /home/docker/sarus-git/build
    cd /home/docker/sarus-git/build
    cmake   -DCMAKE_PREFIX_PATH="/opt/boost/1_65_0;/opt/cpprestsdk/v2.10.0;/opt/libarchive/3.3.1;/opt/rapidjson/rapidjson-master" \
            -Dcpprestsdk_INCLUDE_DIR=/opt/cpprestsdk/v2.10.0/include \
            -DBUILD_STATIC=TRUE \
            .. \
            > cmake_stdout.txt
    cleanup_and_exit_if_last_command_failed
    log "    Config successful, checking version string"
    version_from_cmake=$(cat cmake_stdout.txt | grep "Sarus version" | awk -F ": " '{print $2}')
    version_from_git=$(git describe --tags --dirty)
    log "    Version from CMake: ${version_from_cmake}"
    log "    Version from git : ${version_from_git}" 
    [ "$version_from_git" == "$version_from_cmake" ]
    cleanup_and_exit_if_last_command_failed
    log "    Check successful"
}

log "Checking CMake version detection"
check_static_snapshot
check_git_repo
cleanup
