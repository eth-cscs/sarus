#! /bin/echo This file is meant to be sourced
#
# Collection of functions to be used both by the CI/CD pipelines and
# by the local development.
#

# import utility_functions
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "${script_dir}/utility_functions.bash"

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
        fail_on_error "failed to build ${image_build} image"
        DOCKER_BUILDKIT=${BK} docker build --build-arg BUILDKIT_INLINE_CACHE=${BKIC} -t ${image_run} -f ${script_dir}/${dockerfile_run} ${script_dir}
        fail_on_error "failed to build ${image_run} image"
    else
        local image_build_repo=$(echo ${image_build} | cut -d':' -f1)
        local image_run_repo=$(echo ${image_run} | cut -d':' -f1)
        DOCKER_BUILDKIT=${BK} docker build --cache-from ${image_build_repo}:develop -t ${image_build} -f ${script_dir}/${dockerfile_build} ${script_dir}
        fail_on_error "failed to build ${image_build} image"
        DOCKER_BUILDKIT=${BK} docker build --cache-from ${image_run_repo}:develop -t ${image_run} -f ${script_dir}/${dockerfile_run} ${script_dir}
        fail_on_error "failed to build ${image_run} image"
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

    _run_cmd_in_container ${image_build} docker "bash -i -c '
        . /sarus-source/CI/utility_functions.bash && \
        change_uid_gid_of_docker_user ${host_uid} ${host_gid} && \
        build_sarus ${build_type} ${toolchain_file} $(_build_dir_container) /opt/sarus/default'"
    fail_on_error "${FUNCNAME}: failed to build Sarus"
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

    _copy_cached_build_artifacts_if_available ${cache_dir_host} $(_build_dir_host)/dep

    _run_cmd_in_container ${image_build} docker ". /sarus-source/CI/utility_functions.bash && build_sarus_archive ${build_type} ${toolchain_file} $(_build_dir_container)"
    fail_on_error "${FUNCNAME}: failed to build archive"
}

sarus-test() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    sarus-utest
    sarus-itest-standalone
    sarus-dtest
    sarus-run-gcov
}

sarus-utest() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        run_unit_tests $(_build_dir_container)"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-utest-debug() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    _run_cmd_in_interactive_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        bash"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-itest-standalone() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    local sarus_archive=$(_build_dir_container)/sarus-${build_type}.tar.gz

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        install_sarus_from_archive /opt/sarus ${sarus_archive} && \
        run_integration_tests $(_build_dir_container)"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-itest-from-scratch() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    if [ ${toolchain_file} = "gcc-asan.cmake" ]; then
        echo "${FUNCNAME}: skipping integration tests (Sarus doesn't work when built with ASan)"
        return
    fi

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        install_sarus $(_build_dir_container) /opt/sarus/default && \
        run_integration_tests $(_build_dir_container)"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-itest-from-scratch-debug() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    if [ ${toolchain_file} = "gcc-asan.cmake" ]; then
        echo "${FUNCNAME}: skipping integration tests (Sarus doesn't work when built with ASan)"
        return
    fi

    _run_cmd_in_interactive_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        install_sarus $(_build_dir_container) /opt/sarus/default && \
        bash"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-dtest() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    local sarus_archive=$(_build_dir_container)/sarus-${build_type}.tar.gz

    local cache_oci_hooks_dir=$(_cache_oci_hooks_dir ${cache_dir_host})
    mkdir -p ${cache_oci_hooks_dir}

    local cache_local_repo_dir=$(_cache_local_repo_dir ${cache_dir_host})
    mkdir -p ${cache_local_repo_dir}

    run_distributed_tests ${sarus_source_dir_host} ${sarus_archive} ${cache_oci_hooks_dir} ${cache_local_repo_dir}
    fail_on_error "${FUNCNAME}: failed"
}

sarus-run-gcov() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    if [ ${toolchain_file} != "gcc-gcov.cmake" ]; then
        echo "${FUNCNAME}: skipping gcov execution (Sarus was not built with gcov instrumentation)"
        return
    fi

    _run_cmd_in_container ${image_run} root \
        ". /sarus-source/CI/utility_functions.bash && \
        run_gcov $(_build_dir_container)"

    fail_on_error "${FUNCNAME}: failed"
}

sarus-spack-test() {
    echo "${FUNCNAME^^} with:"
    _print_parameters

    docker run --tty --rm --privileged --user root \
        --mount=src=${sarus_source_dir_host},dst=/sarus-source,type=bind \
        ${image_run} \
        /sarus-source/CI/run_spack_package_test.sh
    fail_on_error "${FUNCNAME}: failed to execute 'run_spack_package_test.sh' in container"
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

        for distro in "${spack_distros[@]}"; do
            docker push ${spack_repo_base}-${distro}:${branch}
        done
    fi
}

sarus-cleanup-images() {
    local branch=$(_git_branch)
    if [ "${branch}" = "master" ] || [ "${branch}" = "develop" ]; then
        return
    fi

    docker rmi ${image_build}
    if [ ${image_run} != ${image_build} ]; then
        docker rmi ${image_run} || true

        for distro in "${spack_distros[@]}"; do
            docker rmi ${spack_repo_base}-${distro}:${branch} || true
        done
    fi
}

_run_cmd_in_container() {
    local image=${1}; shift || error "${FUNCNAME}: missing image argument"
    local user=${1}; shift || error "${FUNCNAME}: missing user argument"
    local command=${1}; shift || error "${FUNCNAME}: missing command argument"
    local additional_docker_opts=${1}; shift || additional_docker_opts=""

    local host_uid=$(id -u)
    local host_gid=$(id -g)

    local cache_oci_hooks_dir=$(_cache_oci_hooks_dir ${cache_dir_host})
    local cache_local_repo_dir=$(_cache_local_repo_dir ${cache_dir_host})
    local cache_centralized_repo_dir=$(_cache_centralized_repo_dir ${cache_dir_host})
    mkdir -p ${cache_oci_hooks_dir}
    mkdir -p ${cache_local_repo_dir}
    mkdir -p ${cache_centralized_repo_dir}

    local docker_options="--rm --tty --privileged --user root \
                          --mount=src=${sarus_source_dir_host},dst=/sarus-source,type=bind \
                          --mount=src=${cache_oci_hooks_dir},dst=/home/docker/.oci-hooks,type=bind \
                          --mount=src=${cache_local_repo_dir},dst=/home/docker/.sarus,type=bind \
                          --mount=src=${cache_centralized_repo_dir},dst=/var/sarus/centralized_repository,type=bind \
                          -e CI_COMMIT_TAG \
                          -e TRAVIS_TAG ${additional_docker_opts}"

    docker run ${docker_options} \
        ${image} bash -c "
        . /sarus-source/CI/utility_functions.bash \
        && change_uid_gid_of_docker_user ${host_uid} ${host_gid} \
        && sudo -E -u ${user} bash -c \"${command}\""
    fail_on_error "${FUNCNAME}: failed to execute '${command}' in container"
}

_run_cmd_in_interactive_container() {
    local image=${1}; shift || error "${FUNCNAME}: missing image argument"
    local user=${1}; shift || error "${FUNCNAME}: missing user argument"
    local command=${1}; shift || error "${FUNCNAME}: missing command argument"

    _run_cmd_in_container "${image}" "${user}" "${command}" "--interactive"
}


_print_parameters() {
    echo "build_type = ${build_type}"
    echo "toolchain_file = ${toolchain_file}"
    echo "sarus_source_dir_host = ${sarus_source_dir_host}"
    echo "cache_dir_host = ${cache_dir_host}"
    echo "dockerfile_build = ${dockerfile_build}"
    echo "image_build = ${image_build}"
    echo "dockerfile_run = ${dockerfile_run}"
    echo "image_run = ${image_run}"
}

_setup_cache_dirs() {
    if [ $(_git_branch) != "develop" ]; then
        local new_cache_dir_host=${sarus_source_dir_host}/cache/$(_job_id_string)

        _copy_cache_dir $(_cache_oci_hooks_dir ${cache_dir_host}) $(_cache_oci_hooks_dir ${new_cache_dir_host})
        _copy_cache_dir $(_cache_local_repo_dir ${cache_dir_host}) $(_cache_local_repo_dir ${new_cache_dir_host})
        _copy_cache_dir $(_cache_centralized_repo_dir ${cache_dir_host}) $(_cache_centralized_repo_dir ${new_cache_dir_host})
        _copy_cached_build_artifacts_if_available ${cache_dir_host} ${new_cache_dir_host}

        export cache_dir_host=${new_cache_dir_host}

    elif [ ! -d ${cache_dir_host} ]; then
        mkdir -pv $(_cache_oci_hooks_dir ${cache_dir_host})
        mkdir -pv $(_cache_local_repo_dir ${cache_dir_host})
        mkdir -pv $(_cache_centralized_repo_dir ${cache_dir_host})
    fi
}

_copy_cache_dir() {
    local from=${1}; shift || error "${FUNCNAME}: missing from argument"
    local to=${1}; shift || error "${FUNCNAME}: missing to argument"

    echo "Setting up cache dir at ${to} (rsync from ${from})"
    mkdir -pv ${from}
    mkdir -pv ${to}
    rsync -var ${from}/ ${to}
    fail_on_error "failed to rsync cache directory"
}

_copy_cached_build_artifacts_if_available() {
    local from=${1}; shift || error "${FUNCNAME}: missing from argument"
    local to=${1}; shift || error "${FUNCNAME}: missing to argument"

    mkdir -p ${to}

    if [ -e ${from}/dropbearmulti ]; then
        cp ${from}/dropbearmulti ${to}/dropbearmulti
        echo "Copied ${from}/dropbearmulti to ${to}/dropbearmulti"
    fi

    if [ -e ${from}/cpputest ]; then
        cp -r ${from}/cpputest ${to}/cpputest
        echo "Copied ${from}/cpputest to ${to}/cpputest"
    fi
}

_cache_oci_hooks_dir() {
    local cache_dir=${1}; shift || error "${FUNCNAME}: missing cache_dir argument"
    echo "${cache_dir}/oci_hooks--$(_job_id_string)"
}

_cache_local_repo_dir() {
    local cache_dir=${1}; shift || error "${FUNCNAME}: missing cache_dir argument"
    echo "${cache_dir}/local_repository--$(_job_id_string)"
}

_cache_centralized_repo_dir() {
    local cache_dir=${1}; shift || error "${FUNCNAME}: missing cache_dir argument"
    echo "${cache_dir}/centralized_repository--$(_job_id_string)"
}

_build_dir_host() {
    echo "${sarus_source_dir_host}/build--$(_job_id_string)"
}

_build_dir_container() {
    echo "/sarus-source/build--$(_job_id_string)"
}

_git_branch() {
    local git_branch=""
    if [ -n "${TRAVIS_BRANCH}" ]; then
        git_branch=${TRAVIS_BRANCH}  # TravisCI
    else
        git_branch=$(git symbolic-ref --short -q HEAD) || git_branch=${CI_COMMIT_REF_NAME} || git_branch="git-detached"
    fi
    echo "${git_branch}"
}

_job_id_string() {
    # The base cache directory is copied into the sarus' source directory (not shared
    # with CI pipelines triggered by other branches), hence we only need to identify the
    # cache directory with a "job ID string" (currently built from <image, build_type>)
    # to avoid clashes between concurrent jobs that belong to the same pipeline.
    # We don't need to care about pipelines triggered by other branches.

    # The CI pipeline triggered by the develop branch is an exception. In that case
    # the pipeline accesses and modifies the base cache directory directly.
    # Beware that clashes may occur if multiple develop pipelines run concurrenlty.
    # We agreed that this is a not-so-important corner case for the time being.

    echo "$(_replace_invalid_chars ${image_run})--${build_type}--$(_replace_invalid_chars ${toolchain_file})"
}

_replace_invalid_chars() {
    local string=${1}; shift || error "${FUNCNAME}: missing string argument"
    echo ${string} |tr '/' '_' |tr ':' '_'
}

export build_type=${1-"Debug"}; shift || true
# Note: the "gcc-gcov.cmake" toolchain file also has the side effect
# of triggering the executing of gcov, i.e. output code coverage metrics
export toolchain_file=${1-"gcc.cmake"}; shift || true
export sarus_source_dir_host=${1-${PWD}}; shift || true
export cache_dir_host=${1-${sarus_source_dir_host}/cache-base}; shift || true
export dockerfile_build=Dockerfile.${1-"standalone-build"}
export image_build=ethcscs/sarus-ci-$(_replace_invalid_chars ${1-"standalone-build"}):$(_git_branch); shift || true
export dockerfile_run=Dockerfile.${1-"standalone-run"}
export image_run=ethcscs/sarus-ci-$(_replace_invalid_chars ${1-"standalone-run"}):$(_git_branch); shift || true
export spack_distros=(ubuntu18.04 debian10 centos7 fedora34 opensuseleap15.2)
export spack_repo_base=ethcscs/sarus-ci-spack

echo "INITIALIZED $(basename "${BASH_SOURCE[0]}") with:"
_print_parameters

_setup_cache_dirs
