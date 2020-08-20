#!/bin/bash

# This script starts a virtual cluster through docker-compose and then
# runs the Sarus's integration tests specific to the virtual cluster.

utilities_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "${utilities_dir}/utility_functions.bash"

script_dir=$(cd $(dirname "$0") && pwd)
cd $script_dir

artifact_name=$1; shift || error "missing artifact_name argument"
cached_oci_hooks_dir=$1; shift || error "missing cache_oci_hooks_dir argument"
cached_local_repo_dir=$1; shift || error "missing cached_local_repo_dir argument"

virtual_cluster_dir=

log() {
    local message=$1
    echo "[ LOG ]  $message"
}

stop_and_remove_cluster() {
    if [ -e $virtual_cluster_dir ]; then
        log "stopping virtual cluster"
        cd $virtual_cluster_dir
        docker-compose down -v
        log "successfully stopped virtual cluster"
        log "removing virtual cluster"
        cd ..
        rm -rf $virtual_cluster_dir
        log "successfully removed virtual cluster"
    fi
}

create_cluster_folder_with_unique_id() {
    local random_id=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
    virtual_cluster_dir=$script_dir/virtual_cluster-$random_id
    mkdir -p $virtual_cluster_dir
    cp $script_dir/docker-compose.yml $virtual_cluster_dir/
}

adapt_docker_compose_file() {
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s/@host_uid@/$(id -u)/g"
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s/@host_gid@/$(id -g)/g"
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s#@cached_oci_hooks_dir@#${cached_oci_hooks_dir}#g"
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s#@cached_local_repo_dir@#${cached_local_repo_dir}#g"
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s#@artifact_name@#${artifact_name}#g"
}

start_cluster() {
    log "starting virtual cluster"
    cd $virtual_cluster_dir
    mkdir -p sync
    docker-compose up -d
    fail_on_error "failed to start cluster with docker-compose up"
    log "successfully started virtual cluster"
}

wait_for_controller_node_to_finish_startup() {
    log "waiting for controller node to finish startup"
    while [ ! -e $virtual_cluster_dir/sync/controller-start-finished ]; do
        sleep 1
    done
    log "successfully waited for controller node to finish startup"
}

wait_for_idle_state_of_server_nodes() {
    log "waiting for idle state of server nodes"
    cd $virtual_cluster_dir
    while ! docker-compose exec -T controller sinfo |grep "server\[0-1\]" |grep idle >/dev/null; do
        sleep 1
    done
    log "successfully waited for idle state of server nodes"
}

run_tests() {
    log "running integration tests in virtual cluster"
    local test_files=$(cd $script_dir/src/integration_tests_for_virtual_cluster && ls test_*.py)
    local tests_dir_in_container=/sarus-source/CI/src
    cd $virtual_cluster_dir
    docker-compose exec --user=docker -T controller bash -c "cd $tests_dir_in_container/integration_tests_for_virtual_cluster && PATH=/opt/sarus/default/bin:\$PATH PYTHONPATH=$tests_dir_in_container:\$PYTHONPATH CMAKE_INSTALL_PREFIX=/opt/sarus/default pytest -v $test_files"
    fail_on_error "failed to run integration tests in virtual cluster"
    log "successfully run integration tests in virtual cluster"
}

create_cluster_folder_with_unique_id
adapt_docker_compose_file
start_cluster
wait_for_controller_node_to_finish_startup
wait_for_idle_state_of_server_nodes
run_tests
stop_and_remove_cluster
