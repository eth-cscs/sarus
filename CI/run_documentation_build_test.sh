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

check_links() {
    log "    Check html links in docs. No broken nor permanent redirects are welcome."
    make html SPHINXOPTS="-W -b linkcheck"
    cleanup_and_exit_if_last_command_failed
}

check_docs() {
    local expected_version=$1
    make html SPHINXOPTS="-W"
    cleanup_and_exit_if_last_command_failed
    log "    Build successful, checking version string"
    version_from_html=$(cat _build/html/index.html | grep "<strong>release</strong>" | awk -F ": " '{print $2}')
    log "    Version from HTML: ${version_from_html}"
    log "    Expected Version: ${expected_version}"
    [ "$expected_version" == "$version_from_html" ]
    cleanup_and_exit_if_last_command_failed
    log "    Check successful"
}

check_git_repo() {
    log "Building documentation from git repository"
    cp -rT $sarus_src_dir /home/docker/sarus-git
    cd /home/docker/sarus-git/doc
    version_from_git=$(git describe --tags --dirty)
    check_docs ${version_from_git}
}

log "Checking documentation build process"
check_git_repo
cleanup
