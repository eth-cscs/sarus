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
    log "Building documentation from static snapshot"
    mkdir /home/docker/sarus-static
    cp -r $sarus_src_dir/* /home/docker/sarus-static
    cd /home/docker/sarus-static/doc
    sphinx-build -b html -W . _build/html
    cleanup_and_exit_if_last_command_failed
    log "    Build successful, checking version string"
    version_from_html=$(cat _build/html/index.html | grep "<strong>release</strong>" | awk -F ": " '{print $2}')
    version_from_file=$(cat ../VERSION)
    log "    Version from HTML: ${version_from_html}"
    log "    Version from file: ${version_from_file}"
    [ "$version_from_file" == "$version_from_html" ]
    cleanup_and_exit_if_last_command_failed
    log "    Check successful"
}

check_git_repo() {
    log "Building documentation from git repository"
    cp -rT $sarus_src_dir /home/docker/sarus-git
    cd /home/docker/sarus-git/doc
    sphinx-build -b html -W . _build/html
    cleanup_and_exit_if_last_command_failed
    log "    Build successful, checking version string"
    version_from_html=$(cat _build/html/index.html | grep "<strong>release</strong>" | awk -F ": " '{print $2}')
    version_from_git=$(git describe --tags --dirty)
    log "    Version from HTML: ${version_from_html}"
    log "    Version from git : ${version_from_git}"
    [ "$version_from_git" == "$version_from_html" ]
    cleanup_and_exit_if_last_command_failed
    log "    Check successful"
}

log "Checking documentation build process"
check_static_snapshot
check_git_repo
cleanup
