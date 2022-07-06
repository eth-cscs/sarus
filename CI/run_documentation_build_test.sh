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

check_links() {
    log "    Check html links in docs. No broken nor permanent redirects are welcome."
    make html SPHINXOPTS="-W -b linkcheck"
    fail_on_error "links check failed"
}

check_docs() {
    local expected_version=$1
    make html SPHINXOPTS="-W"
    fail_on_error "failed to make html"
    log "    Build successful, checking version string"
    version_from_html=$(cat _build/html/index.html | grep "<strong>release</strong>" | awk -F ": " '{print $2}')
    log "    Version from HTML: ${version_from_html}"
    log "    Expected Version: ${expected_version}"
    [ "$expected_version" == "$version_from_html" ]
    fail_on_error "$expected_version != $version_from_html"
    log "    Check successful"
}

check_git_repo() {
    log "Building documentation from git repository"
    cp -rT $sarus_src_dir /home/docker/sarus-git
    cd /home/docker/sarus-git/doc
    version_from_git=$(git describe --tags --dirty --always)
    check_docs ${version_from_git}
}

log "Checking documentation build process"
check_git_repo
cleanup
