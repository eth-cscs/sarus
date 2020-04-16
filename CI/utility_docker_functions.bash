#! /bin/echo This file is meant to be sourced
#
# Collection of functions to be used both by the CI/CD pipelines and
# by the local development.
#

# import utility_functions
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "${script_dir}/utility_functions.bash"

_git_branch() {
    local git_branch=""
    if [ -n "${TRAVIS_BRANCH}" ]; then
        git_branch=${TRAVIS_BRANCH}  # TravisCI
    else
        git_branch=$(git symbolic-ref --short -q HEAD) || git_branch=${CI_COMMIT_REF_NAME} || git_branch="git-detached"
    fi
    echo "${git_branch}"
}

_replace_invalid_chars() {
    local string=${1}; shift || error "${FUNCNAME}: missing string argument"
    echo ${string} |tr '/' '_' |tr ':' '_'
}

_setup_cache_dir() {
    if [ $(_git_branch) != "develop" ]; then
        local new_cache_dir_host=${sarus_source_dir_host}/cache/$(_replace_invalid_chars ${image_run})--${build_type}

        echo "Setting up cache dir at ${new_cache_dir_host} (copy from ${cache_dir_host})"
        mkdir -p ${cache_dir_host}
        mkdir -p ${new_cache_dir_host}
        rsync -ar ${cache_dir_host}/ ${new_cache_dir_host}
        fail_on_error "failed to rsync cache directory"
        export cache_dir_host=${new_cache_dir_host}
    fi
}

_print_parameters() {
    echo "build_type = ${build_type}"
    echo "sarus_source_dir_host = ${sarus_source_dir_host}"
    echo "cache_dir_host = ${cache_dir_host}"
    echo "dockerfile_build = ${dockerfile_build}"
    echo "image_build = ${image_build}"
    echo "dockerfile_run = ${dockerfile_run}"
    echo "image_run = ${image_run}"
}

export build_type=${1-"Debug"}; shift || true
export sarus_source_dir_host=${1-${PWD}}; shift || true
export cache_dir_host=${1-${sarus_source_dir_host}/cache-base}; shift || true
export dockerfile_build=Dockerfile.${1-"standalone-build"}
export image_build=ethcscs/sarus-ci-$(_replace_invalid_chars ${1-"standalone-build"}):$(_git_branch); shift || true
export dockerfile_run=Dockerfile.${1-"standalone-run"}
export image_run=ethcscs/sarus-ci-$(_replace_invalid_chars ${1-"standalone-run"}):$(_git_branch); shift || true

_setup_cache_dir

echo "INITIALIZED $(basename "${BASH_SOURCE[0]}") with:"
_print_parameters

sarus-build-images() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    # Travis is not supporting buildkit at the moment :(
    local BK=1
    local BKIC=1
    if [ "${TRAVIS}" == "true" ]; then
        BK=0
        BKIC=0
    fi

    # Use develop as build cache source
    if [ "$(_git_branch)" == "develop" ]; then
        DOCKER_BUILDKIT=${BK} docker build --build-arg BUILDKIT_INLINE_CACHE=${BKIC} -t ${image_build} -f ${script_dir}/${dockerfile_build} ${script_dir}
        fail_on_error "failed to build sarus build image"
        DOCKER_BUILDKIT=${BK} docker build --build-arg BUILDKIT_INLINE_CACHE=${BKIC} -t ${image_run} -f ${script_dir}/${dockerfile_build} ${script_dir}
        fail_on_error "failed to build sarus run image"
    else
        DOCKER_BUILDKIT=${BK} docker build --cache-from ethcscs/sarus-ci-build:develop -t ${image_build} -f ${script_dir}/${dockerfile_build} ${script_dir}
        fail_on_error "failed to build sarus build image"
        DOCKER_BUILDKIT=${BK} docker build --cache-from ethcscs/sarus-ci-run:develop -t ${image_run} -f ${script_dir}/${dockerfile_run} ${script_dir}
        fail_on_error "failed to build sarus run image"
    fi
}

sarus-check-version-and-docs() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    _run_cmd_in_container ${image_build} docker /sarus-source/CI/check_version_from_cmake.sh
    fail_on_error "${FUNCNAME}: check_version_from_cmake failed"
    _run_cmd_in_container ${image_run} docker /sarus-source/CI/run_documentation_build_test.sh
    fail_on_error "${FUNCNAME}: documentation check failed"
}

sarus-build-from-scratch-and-test() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    if [ ${image_build} != ${image_run} ]; then
        error "${FUNCNAME}: error: image_build and image_run must be the same when building from source"
    fi

    sarus-build-from-scratch
    sarus-utest
    sarus-itest-from-scratch
}

sarus-build-from-scratch() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    if [ ${image_build} != ${image_run} ]; then
        error "${FUNCNAME}: error: image_build and image_run must be the same when building from source"
    fi

    local host_uid=$(id -u)
    local host_gid=$(id -g)
    local build_dir_container=$(_build_dir_container ${image_build} ${build_type})

    _run_cmd_in_container ${image_build} docker "bash -i -c '
        . /sarus-source/CI/utility_functions.bash && \
        change_uid_gid_of_docker_user ${host_uid} ${host_gid} && \
        build_sarus ${build_type} ${build_dir_container} /opt/sarus/default'"
    fail_on_error "${FUNCNAME}: failed to build and install Sarus"
}

sarus-build-and-test() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    sarus-build
    sarus-test
}

sarus-build() {
    # Builds Sarus standalone with a Docker Image containing all dependencies.
    # The output goes to the build folder inside ${sarus_source_dir_host}.

    echo "${FUNCNAME^^} with:"
    _print_parameters

    local build_dir_host=$(_build_dir_host ${image_build} ${build_type} ${sarus_source_dir_host})
    local build_dir_container=$(_build_dir_container ${image_build} ${build_type})

    # Use cached build artifacts, if available
    test -e ${cache_dir_host}/openssh.tar \
        && mkdir -p ${build_dir_host}/dep \
        && cp ${cache_dir_host}/openssh.tar ${build_dir_host}/dep/openssh.tar
    test -e ${cache_dir_host}/cpputest \
        && mkdir -p ${build_dir_host}/dep \
        && cp -r ${cache_dir_host}/cpputest ${build_dir_host}/dep/cpputest

    _run_cmd_in_container ${image_build} docker ". /sarus-source/CI/utility_functions.bash && build_sarus_archive ${build_type} ${build_dir_container}"
    fail_on_error "${FUNCNAME}: failed to build archive"
}

sarus-test() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    sarus-utest
    sarus-itest-standalone
    sarus-dtest
}

sarus-utest() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    local build_dir_container=$(_build_dir_container ${image_build} ${build_type})

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        run_unit_tests ${build_dir_container}"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-itest-standalone() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    local build_dir_container=$(_build_dir_container ${image_build} ${build_type})
    local sarus_archive=${build_dir_container}/sarus-${build_type}.tar.gz

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        install_sarus_from_archive /opt/sarus ${sarus_archive} && \
        run_integration_tests ${build_dir_container} ${build_type}"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-itest-from-scratch() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    local build_dir_container=$(_build_dir_container ${image_build} ${build_type})

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        install_sarus ${build_dir_container} /opt/sarus/default && \
        run_integration_tests ${build_dir_container} ${build_type}"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-dtest() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    local build_dir_container=$(_build_dir_container ${image_build} ${build_type})
    local sarus_archive=${build_dir_container}/sarus-${build_type}.tar.gz

    local cache_oci_hooks_dir=$(_cache_oci_hooks_dir)
    mkdir -p ${cache_oci_hooks_dir}

    local cache_local_repo_dir=$(_cache_local_repo_dir)
    mkdir -p ${cache_local_repo_dir}

    run_distributed_tests ${sarus_source_dir_host} ${sarus_archive} ${cache_oci_hooks_dir} ${cache_local_repo_dir}
    fail_on_error "${FUNCNAME}: failed"
}

sarus-publish-images() {
    # Publish to Dockehub
    local branch=$(_git_branch)
    if [ "${branch}" == "master" ] || [ "${branch}" == "develop" ]; then
        if [ "${CI}" ]
        then
            # On Gitlab CI, take credentials from env variables>
            docker login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD
        else
            # Locally, ask for credentials
            docker login
        fi
        docker push ${image_build}
        docker push ${image_run}
    fi
}

sarus-cleanup-images() {
    local branch=$(_git_branch)
    if [ "${branch}" != "master" ] && [ "${branch}" != "develop" ]; then
        docker rmi ${image_build}
        docker rmi ${image_run}
    fi
}

_run_cmd_in_container() {
    local image=${1}; shift || error "${FUNCNAME}: missing image argument"
    local user=${1}; shift || error "${FUNCNAME}: missing user argument"
    local command=${1}; shift || error "${FUNCNAME}: missing command argument"

    local host_uid=$(id -u)
    local host_gid=$(id -g)

    local cache_oci_hooks_dir=$(_cache_oci_hooks_dir)
    local cache_local_repo_dir=$(_cache_local_repo_dir)
    local cache_centralized_repo_dir=$(_cache_centralized_repo_dir)
    mkdir -p ${cache_oci_hooks_dir}
    mkdir -p ${cache_local_repo_dir}
    mkdir -p ${cache_centralized_repo_dir}

    docker run --rm --tty --privileged --user root \
        --mount=src=${sarus_source_dir_host},dst=/sarus-source,type=bind \
        --mount=src=${cache_oci_hooks_dir},dst=/home/docker/.oci-hooks,type=bind \
        --mount=src=${cache_local_repo_dir},dst=/home/docker/.sarus,type=bind \
        --mount=src=${cache_centralized_repo_dir},dst=/var/sarus/centralized_repository,type=bind \
        ${image} bash -c "
        . /sarus-source/CI/utility_functions.bash \
        && change_uid_gid_of_docker_user ${host_uid} ${host_gid} \
        && sudo -u ${user} bash -c \"${command}\""
    fail_on_error "${FUNCNAME}: failed to execute '${command}' in container"
}

_build_dir_host() {
    local image=${1}; shift || error "${FUNCNAME}: missing image argument"
    local build_type=${1}; shift || error "${FUNCNAME}: missing build_type argument"
    local sarus_source_dir_host=${1}; shift || error "${FUNCNAME}: missing sarus_source_dir_host argument"
    echo "${sarus_source_dir_host}/build--$(_replace_invalid_chars ${image})--${build_type}"
}

_build_dir_container() {
    local image=${1}; shift || error "${FUNCNAME}: missing image argument"
    local build_type=${1}; shift || error "${FUNCNAME}: missing build_type argument"
    echo "/sarus-source/build--$(_replace_invalid_chars ${image})--${build_type}"
}

_cache_oci_hooks_dir() {
    echo "${cache_dir_host}/oci-hooks"
}

_cache_local_repo_dir() {
    echo "${cache_dir_host}/local_repository"
}

_cache_centralized_repo_dir() {
    echo "${cache_dir_host}/centralized_repository"
}
