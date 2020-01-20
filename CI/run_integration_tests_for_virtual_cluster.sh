#!/bin/bash

# This script starts a virtual cluster through docker-compose and then
# runs the Sarus's integration tests specific to the virtual cluster.

script_dir=$(cd $(dirname "$0") && pwd)
cd $script_dir

artifact_name=$1; shift
cached_home_dir=$1; shift

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

cleanup_and_exit_if_last_command_failed() {
    local last_command_exit_code=$?
    if [ $last_command_exit_code -ne 0 ]; then
        log "command failed"
        stop_and_remove_cluster
        exit 1
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
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s#@cached_home_dir@#${cached_home_dir}#g"
    sed -i $virtual_cluster_dir/docker-compose.yml -e "s#@artifact_name@#${artifact_name}#g"
}

start_cluster() {
    log "starting virtual cluster"
    cd $virtual_cluster_dir
    mkdir sync
    docker-compose up -d
    cleanup_and_exit_if_last_command_failed
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
    docker-compose exec --user=docker -T controller bash -c "cd $tests_dir_in_container/integration_tests_for_virtual_cluster && PATH=/opt/sarus/default/bin:\$PATH PYTHONPATH=$tests_dir_in_container:\$PYTHONPATH pytest -v $test_files"
    cleanup_and_exit_if_last_command_failed
    log "successfully run integration tests in virtual cluster"
}

create_cluster_folder_with_unique_id
adapt_docker_compose_file
start_cluster
wait_for_controller_node_to_finish_startup
wait_for_idle_state_of_server_nodes
run_tests
stop_and_remove_cluster
