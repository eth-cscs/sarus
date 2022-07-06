#!/bin/bash

utilities_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "${utilities_dir}/utility_functions.bash"

log() {
    local message=$1
    echo "[ LOG ]  $message"
}

cleanup() {
    rm -rf /home/docker/sarus-git
    rm -rf /home/docker/sarus-static
}

sarus_src_dir=/sarus-source

check_git_repo() {
    log "Running CMake from git repository"
    cp -rT $sarus_src_dir /home/docker/sarus-git
    mkdir -p /home/docker/sarus-git/build
    cd /home/docker/sarus-git/build
    cmake -DBUILD_STATIC=TRUE .. > cmake_stdout.txt
    fail_on_error "failed to run CMake on git repo"
    log "    Config successful, checking version string"
    version_from_cmake=$(cat cmake_stdout.txt | grep "Sarus version" | awk -F ": " '{print $2}')
    version_from_git=$(git describe --tags --dirty --always)
    log "    Version from CMake: ${version_from_cmake}"
    log "    Version from git : ${version_from_git}"
    [ "$version_from_git" == "$version_from_cmake" ]
    fail_on_error "$version_from_git != $version_from_cmake"
    log "    Check successful"
}

log "Checking CMake version detection"
check_git_repo
cleanup
