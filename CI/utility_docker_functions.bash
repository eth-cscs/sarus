#! /bin/echo This file is meant to be sourced
#
# Collection of functions to be used both by the CI/CD pipelines and
# by the local development.
#

# import utility_functions
utilities_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "${utilities_dir}/utility_functions.bash"

sarus-build-images() {
    # Use develop as closest cache possible
    docker pull ethcscs/sarus-ci-build:develop || true

    echo "Building Sarus Images with tag $(git_branch)"
    docker build -t $(build_image) -f ./CI/Dockerfile.build ./CI
    fail_on_error "failed to build sarus build image"
    docker build -t $(run_image) -f ./CI/Dockerfile.run ./CI
    fail_on_error "failed to build sarus run image"
}

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
    local sarus_source_dir_on_host=${1-${PWD}}; shift
    local os_image=${1-"ubuntu:18.04"}; shift # [ubuntu:18.04 | debian:10 | centos:7 ]

    local host_uid=$(id -u)
    local host_gid=$(id -g)
    docker run --tty --rm --privileged --user root -v ${sarus_source_dir_on_host}:/sarus-source ${os_image} bash -c "
        /sarus-source/CI/installation/install_sudo.sh ${os_image} && \
        useradd -m docker && echo 'docker:docker' | chpasswd && \
        echo 'docker ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers && \
        . /sarus-source/CI/utility_functions.bash && change_uid_gid_of_docker_user ${host_uid} ${host_gid} && \
        . /sarus-source/CI/installation/install_packages_${os_image}.sh && \
        . /sarus-source/CI/installation/install_dependencies.bash && \
        /usr/bin/sudo -u docker bash --login -c 'cd /sarus-source && \
                                                . /sarus-source/CI/utility_functions.bash && \
                                                build_and_install_sarus Release /sarus-source/build-Release && \
                                                run_smoke_tests'
    "
}

sarus-build-and-test() {
    echo SARUS-BUILD-AND-TEST

    local sarus_source_dir_on_host=${1-$PWD}; shift
    local build_type=${1-"Debug"}; shift
    local build_cache=${1-${sarus_source_dir_on_host}/cache}; shift
    local tests_cache=${1-${sarus_source_dir_on_host}/cache}; shift

    sarus-build ${sarus_source_dir_on_host} ${build_type} ${build_cache}
    sarus-test ${sarus_source_dir_on_host} ${build_type} ${tests_cache}
}

sarus-build() {
    # Builds Sarus standalone with a Docker Image containing all dependencies $(build_image).
    # The build defined by ${build_type} is either Release or Debug.
    # The output goes to a folder called build-${build_type} inside input folder ${sarus_source_dir_on_host}.
    local sarus_source_dir_on_host=${1-${PWD}}; shift
    local build_type=${1-"Debug"}; shift
    local build_cache=${1-${sarus_source_dir_on_host}/cache}; shift

    echo "SARUS-BUILD (${build_type}) with $(build_image) from ${sarus_source_dir_on_host} and cache from ${build_cache}"

    # Use cached build artifacts, if available
    local host_build_dir=${sarus_source_dir_on_host}/build-${build_type}
    test -e ${build_cache}/openssh.tar && mkdir -p ${host_build_dir}/dep && cp ${build_cache}/openssh.tar ${host_build_dir}/dep/openssh.tar
    test -e ${build_cache}/cpputest && mkdir -p ${host_build_dir}/dep && cp -r ${build_cache}/cpputest ${host_build_dir}/dep/cpputest

    local host_uid=$(id -u)
    local host_gid=$(id -g)
    local container_build_dir=/sarus-source/build-${build_type}
    docker run --tty --rm --user root --mount=src=${sarus_source_dir_on_host},dst=/sarus-source,type=bind $(build_image) bash -c "
        . /sarus-source/CI/utility_functions.bash \
        && change_uid_gid_of_docker_user ${host_uid} ${host_gid} \
        && sudo -u docker bash -c '. /sarus-source/CI/utility_functions.bash && build_sarus_archive ${build_type} ${container_build_dir}'"
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

sarus-publish-images() {
    # Publish to Dockehub
    local branch=$(git_branch)
    if [ "${branch}" == "master" ] || [ "${branch}" == "develop" ]; then
        if [ "${CI}" ]
        then
            # On Gitlab CI, take credentials from env variables>
            docker login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD
        else
            # Locally, ask for credentials
            docker login
        fi
        docker push $(build_image)
        docker push $(run_image)
    fi
}

sarus-cleanup-images() {
    local branch=$(git_branch)
    if [ "${branch}" != "master" ] && [ "${branch}" != "develop" ]; then
        docker rmi $(build_image)
        docker rmi $(run_image)
    fi
}