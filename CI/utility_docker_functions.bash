#! /bin/echo This file is meant to be sourced
#
# Collection of functions to be used both by the CI/CD pipelines and
# by the local development.
#

# import utility_functions
utilities_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "${utilities_dir}/utility_functions.bash"


git_branch() {
    local git_branch=""
    git_branch=$(git symbolic-ref --short -q HEAD) || git_branch=${CI_COMMIT_REF_NAME} || git_branch=${TRAVIS_BRANCH} || git_branch="git-detached"
    echo ${git_branch}
}

build_image() {
    local build_image="ethcscs/sarus-ci-build:$(git_branch)"
    echo ${build_image}
}

run_image() {
    local run_image="ethcscs/sarus-ci-run:$(git_branch)"
    echo ${run_image}
}

sarus-check-version-and-docs() {
    local sarus_source_dir_on_host=${1-${PWD}}; shift

    docker run --tty --rm -v ${sarus_source_dir_on_host}:/sarus-source $(build_image) /sarus-source/CI/check_version_from_cmake.sh
    fail_on_error "check_version_from_cmake failed"
    docker run --tty --rm -v ${sarus_source_dir_on_host}:/sarus-source $(run_image) /sarus-source/CI/run_documentation_build_test.sh
    fail_on_error "documentation check failed"
}

sarus-build-from-scratch-and-test() {
    # Test whole installation on a supported vanilla linux distro.
    # os_image=[ubuntu:18.04 | debian:10 | centos:7 ]
    local sarus_source_dir_on_host=${1-${PWD}}; shift
    local os_image=${1-"ubuntu:18.04"}; shift

    # TODO PART 2: Adjust this to reuse utility functions
    docker run --tty --rm --privileged --user root -v ${sarus_source_dir_on_host}:/sarus-source ${os_image} /sarus-source/CI/installation/test_driver.sh ${os_image}
}

sarus-build-and-test() {
    echo SARUS-BUILD-AND-TEST

    local sarus_source_dir_on_host=${1-$PWD}; shift
    local build_type=${1-"Debug"}; shift
    local tests_cache=${1-${sarus_source_dir_on_host}/cache}; shift

    sarus-build ${sarus_source_dir_on_host} ${build_type}
    sarus-test ${sarus_source_dir_on_host} ${build_type} ${tests_cache}
}

sarus-build() {
    # Builds Sarus standalone with a Docker Image containing all dependencies $(build_image).
    # The build defined by ${build_type} is either Release or Debug.
    # The output goes to a folder called build-${build_type} inside input folder ${sarus_source_dir_on_host}.
    local sarus_source_dir_on_host=${1-${PWD}}; shift
    local build_type=${1-"Debug"}; shift

    echo "SARUS-BUILD (${build_type}) with $(build_image) from ${sarus_source_dir_on_host}"

    local build_dir=/sarus-source/build-${build_type}
    local host_uid=$(id -u)
    local host_gid=$(id -g)

    # TODO PART 2: Bring back the build cache of openssh and cppurest tars.

    docker run --tty --rm --user root --mount=src=${sarus_source_dir_on_host},dst=/sarus-source,type=bind $(build_image) bash -c "
        . /sarus-source/CI/utility_functions.bash \
        && change_uid_gid_of_docker_user ${host_uid} ${host_gid} \
        && sudo -u docker bash -c '. /sarus-source/CI/utility_functions.bash && build_sarus_archive ${build_type} ${build_dir}'"
    fail_on_error "sarus-build failed"
}

sarus-test() {
    local sarus_source_dir_on_host=${1-$PWD}; shift
    local build_type=${1-"Debug"}; shift
    local tests_cache=${1-${sarus_source_dir_on_host}/cache}; shift

    echo "SARUS-RUN-ALL-TEST at ${sarus_source_dir_on_host} with $(run_image) and cache from ${tests_cache}"

    sarus-utest ${sarus_source_dir_on_host} ${build_type}
    sarus-itest ${sarus_source_dir_on_host} ${build_type} ${tests_cache}
    sarus-dtest ${sarus_source_dir_on_host} ${build_type} ${tests_cache}
}

sarus-utest() {
    local sarus_source_dir_on_host=${1-$PWD}; shift
    local build_type=${1-"Debug"}; shift

    echo "SARUS-RUN-UNIT-TESTS at ${sarus_source_dir_on_host} with $(run_image)"

    local build_dir=/sarus-source/build-${build_type}
    local host_uid=$(id -u)
    local host_gid=$(id -g)

    docker run --tty --rm --user root --privileged \
        --mount=src=${sarus_source_dir_on_host},dst=/sarus-source,type=bind \
        $(run_image) \
        bash -c ". /sarus-source/CI/utility_functions.bash && \
                change_uid_gid_of_docker_user ${host_uid} ${host_gid} && \
                run_unit_tests ${build_dir}"
    fail_on_error "sarus-utest failed"
}

sarus-itest() {
    local sarus_source_dir_on_host=${1-$PWD}; shift
    local build_type=${1-"Debug"}; shift
    local tests_cache=${1-${sarus_source_dir_on_host}/cache}; shift

    echo "SARUS-RUN-INTEGRATION-TESTS at ${sarus_source_dir_on_host} with $(run_image) and cache from ${tests_cache}"

    local build_dir=/sarus-source/build-${build_type}
    local host_uid=$(id -u)
    local host_gid=$(id -g)

    local cache_home=${tests_cache}/home_${build_type}
    local cache_centralized_repository_dir=${tests_cache}/centralized_repository_${build_type}
    mkdir -p ${cache_centralized_repository_dir}
    mkdir -p ${cache_home}

    docker run --tty --rm --user root --privileged \
        --mount=src=${sarus_source_dir_on_host},dst=/sarus-source,type=bind \
        --mount=src=${cache_home},dst=/home/docker,type=bind \
        --mount=src=${cache_centralized_repository_dir},dst=/var/sarus/centralized_repository,type=bind \
        $(run_image) \
        bash -c ". /sarus-source/CI/utility_functions.bash && \
                change_uid_gid_of_docker_user ${host_uid} ${host_gid} && \
                run_integration_tests ${build_dir} ${build_type}"
    fail_on_error "sarus-itest failed"
}

sarus-dtest() {
    local sarus_source_dir_on_host=${1-$PWD}; shift
    local build_type=${1-"Debug"}; shift
    local tests_cache=${1-${sarus_source_dir_on_host}/cache}; shift

    echo "SARUS-RUN-DISTRIBUTED-TESTS at ${sarus_source_dir_on_host} with $(run_image) and cache from ${tests_cache}"

    local cache_home=${tests_cache}/home_${build_type}
    mkdir -p ${cache_home}

    run_distributed_tests ${sarus_source_dir_on_host} ${build_type} ${cache_home}
    fail_on_error "sarus-dtest failed"
}

sarus-build-images() {
    # Use develop as closest cache possible
    docker pull ethcscs/sarus-ci-build:develop || true

    echo "Building Sarus Images with tag $(git_branch)"
    docker build -t $(build_image) -f ./CI/Dockerfile.build ./CI
    docker build -t $(run_image) -f ./CI/Dockerfile.run ./CI
}

sarus-publish-images() {
    # TODO PART 2
    # Publish to Dockehub
    echo TODO
}