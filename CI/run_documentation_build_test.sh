#!/bin/bash

log() {
    local message=$1
    echo "[ LOG ]  $message"
}

cleanup_and_exit_if_last_command_failed() {
    local last_command_exit_code=$?
    if [ $last_command_exit_code -ne 0 ]; then
        log "command failed"
        exit 1
    fi
}

sarus_src_dir=/sarus-source

install_documentation_tools() {
    log "Installing documentation tools"
    cd /home/docker
    python3 -m venv ./venv
    source venv/bin/activate
    pip install sphinx sphinx-rtd-theme
    cleanup_and_exit_if_last_command_failed
}

check_static_snapshot() {
    log "Building documentation from static snapshot"
    mkdir /home/docker/sarus-static
    cp -r $sarus_src_dir/* sarus-static
    cd sarus-static/doc
    make html
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
    cd $sarus_src_dir/doc
    make html
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
install_documentation_tools
check_static_snapshot
check_git_repo
