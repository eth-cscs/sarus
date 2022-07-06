#! /bin/echo This file is meant to be sourced

error() {
    local message=${1}
    echo "${message}"
    exit 1
}

fail_on_error() {
    local last_command_exit_code=$?
    local message=$1
    if [ ${last_command_exit_code} -ne 0 ]; then
        error "${message}"
    fi
}

change_uid_gid_of_docker_user() {
    local host_uid=$1; shift || error "${FUNCNAME}: missing host_uid argument"
    local host_gid=$1; shift || error "${FUNCNAME}: missing host_gid argument"

    local container_uid=$(id -u docker)
    local container_gid=$(id -g docker)

    if [ ${host_uid} -eq ${container_uid} ] && [ ${host_gid} -eq ${container_gid} ]; then
        echo "Not changing UID/GID of Docker user (container's UID/GID already match with host)"
        return
    fi

    echo "Changing UID/GID of Docker user"
    sudo usermod -u $host_uid docker
    sudo groupmod -g $host_gid docker
    sudo find / -path /proc -prune -o -user docker -exec chown -h $host_uid {} \;
    sudo find / -path /proc -prune -o -group docker -exec chgrp -h $host_gid {} \;
    sudo usermod -g $host_gid docker
    sudo usermod -d /home/docker docker
    echo "Successfully changed UID/GID of Docker user"
}

build_sarus() {
    local build_type=${1}; shift || error "${FUNCNAME}: missing build_type argument"
    local toolchain_file=${1}; shift || error "${FUNCNAME}: missing toolchain_file argument"
    local build_dir=${1}; shift || error "${FUNCNAME}: missing build_dir argument"
    local install_dir=${1}; shift || error "${FUNCNAME}: missing install_dir argument"

    # DOCS: Create a new folder
    mkdir -p ${build_dir} && cd ${build_dir}

    # DOCS: Configure and build
    cmake -DCMAKE_TOOLCHAIN_FILE=${build_dir}/../cmake/toolchain_files/${toolchain_file} \
        -DCMAKE_BUILD_TYPE=${build_type} \
        -DCMAKE_INSTALL_PREFIX=${install_dir} \
        ..

    make -j$(nproc)
    # DOCS: EO Configure and build
}

install_sarus() {
    local build_dir=${1}; shift || error "${FUNCNAME}: missing build_dir argument"
    local install_dir=${1}; shift || error "${FUNCNAME}: missing install_dir argument"

    cd ${build_dir}

    # DOCS: Install Sarus
    sudo make install

    # DOCS: Create OCI bundle dir
    sudo mkdir -p ${install_dir}/var/OCIBundleDir

    # DOCS: Finalize installation
    sudo cp /usr/local/bin/tini-static-amd64 ${install_dir}/bin || true
    sudo cp /usr/local/bin/runc.amd64 ${install_dir}/bin || true

    sudo ${install_dir}/configure_installation.sh
    export PATH=${install_dir}/bin:${PATH}

    # DOCS: EO Finalize installation
}

build_sarus_archive() {
    # Build and package Sarus as a standalone archive ready for installation
    local build_type=${1}; shift || error "${FUNCNAME}: missing build_type argument"
    local toolchain_file=${1}; shift || error "${FUNCNAME}: missing toolchain_file argument"
    local build_dir=${1}; shift || error "${FUNCNAME}: missing build_dir argument"

    echo "Building Sarus (${build_type} with ${toolchain_file}) in ${build_dir}"

    mkdir -p ${build_dir} && cd ${build_dir}

    local git_description=$(git describe --tags --dirty --always || echo "git_version_not_available")
    local prefix_dir=${build_dir}/install/${git_description}-${build_type}
    cmake   -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/${toolchain_file} \
            -DCMAKE_BUILD_TYPE=${build_type} \
            -DBUILD_STATIC=TRUE \
            -DENABLE_TESTS_WITH_VALGRIND=FALSE \
            -DENABLE_SSH=TRUE \
            -DCMAKE_INSTALL_PREFIX=${prefix_dir} \
            ..
    make -j$(nproc)

    fail_on_error "Failed to build Sarus (cmake + make)"

    echo "Successfully built Sarus"

    # Build archive
    local archive_name="sarus-${build_type}.tar.gz"
    echo "Building archive ${archive_name}"

    rm -rf ${build_dir}/install
    make install
    fail_on_error "Failed to make install Sarus"

    # Adjust standalone README instructions
    version=${CI_COMMIT_TAG-${GITHUB_REF_NAME-"1.0.0"}}
    sed -e "s|@SARUS_VERSION@|${version}|g" ${build_dir}/../standalone/README.md.in > ${build_dir}/../standalone/README.md
    cp -v ${build_dir}/../standalone/README.md ${build_dir}/README.md
    fail_on_error "Failed to prepare README.md for Sarus archive"

    rsync -arvL --chmod=go-w --exclude=*.in ${build_dir}/../standalone/ ${prefix_dir}/

    # runc is statically linked with libseccomp, which is licensed under GNU LGPL-2.1.
    # To comply with LGPL-2.1 (§6(a)), include the libseccomp source code in the licenses folder.
    wget -O ${prefix_dir}/licenses/libseccomp-2.5.1.tar.gz https://github.com/opencontainers/runc/releases/download/v1.0.3/libseccomp-2.5.1.tar.gz

    mkdir -pv ${prefix_dir}/var/OCIBundleDir

    # Bring cached binaries if available (see Dockerfile.build)
    cp /usr/local/bin/tini-static-amd64 ${prefix_dir}/bin || true
    cp /usr/local/bin/runc.amd64 ${prefix_dir}/bin || true
    cp /usr/local/bin/skopeo ${prefix_dir}/bin || true
    cp /usr/local/bin/umoci ${prefix_dir}/bin || true

    # Tar archive
    cd ${prefix_dir}/.. && tar cz --owner=root --group=root --file=../${archive_name} *
    cp  ${build_dir}/${archive_name} ${build_dir}/../${archive_name}

    echo "Prepare RELEASE_NOTES for CI to use"
    python3 ${build_dir}/../CI/create_release_notes.py
    fail_on_error "Failed to prepare RELEASE_NOTES.md for Sarus archive"
    cp -v ${build_dir}/../RELEASE_NOTES.md ${build_dir}/RELEASE_NOTES.md

    echo "Successfully built Sarus sarchive"
}

install_sarus_from_archive() {
    local root_dir=$1; shift
    local archive=$1; shift
    local pwd_backup=$(pwd)

    echo "Installing Sarus from archive ${archive}"

    sudo mkdir -p ${root_dir}
    fail_on_error "Failed to install Sarus"
    cd ${root_dir}
    sudo rm -rf * #remove any other installation
    sudo tar xf ${archive}
    fail_on_error "Failed to install Sarus"
    local sarus_version=$(ls)
    sudo ln -s ${sarus_version} default
    fail_on_error "Failed to install Sarus"

    export PATH=/opt/sarus/default/bin:${PATH}
    sudo /opt/sarus/default/configure_installation.sh

    fail_on_error "Failed to install Sarus"

    echo "Successfully installed Sarus"

    cd ${pwd_backup}
}

run_sarus_registry() {
    # copy mounted directory to get around needing write permission and prevent writing to host machine as root
    sarus pull registry:2
    sarus run --mount=type=bind,src=/var/local_registry,dst=/var/local_registry,readonly \
    registry:2 sh -c "cp -r /var/local_registry /var/lib/registry && \
    registry serve /etc/docker/registry/config.yml" \
    > /dev/null 2>&1 &
}

run_unit_tests() {
    local build_dir=$1; shift || error "${FUNCNAME}: missing build_dir argument"
    cd ${build_dir}

    # Note: use CI/LSan.supp to suppress errors about memory leaks in
    # Sarus dependencies, e.g. OpenSSL. Such leaks are out of our control.

    sudo -u docker CTEST_OUTPUT_ON_FAILURE=1 ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=${build_dir}/../CI/LSan.supp ctest --exclude-regex AsRoot
    fail_on_error "Unit tests as normal user failed"

    CTEST_OUTPUT_ON_FAILURE=1 ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=${build_dir}/../CI/LSan.supp ctest --tests-regex AsRoot # Add as many regex as you want with -R. Allow output with -VV.
    # TODO: allow args forwarding to cpputest so we can run specific testswithin the filtered file with -n for example.
    fail_on_error "Unit tests as root user failed"

    # Adjust ownership of coverage files (Debug only)
    # after execution of unit tests as root
    find ${build_dir} -name "*.gcda" -exec sudo chown docker:docker {} \;
    fail_on_error "Failed to chown *.gcda files (necessary to update code coverage as non-root user)"
}

run_integration_tests() {
    local build_dir=$1; shift || error "${FUNCNAME}: missing build_dir argument"

    echo "Running integration tests with user=docker"
    sudo -u docker --login bash -c "
        PATH=/opt/sarus/default/bin:\${PATH} PYTHONPATH=/sarus-source/CI/src:\${PYTHONPATH} \
        CMAKE_INSTALL_PREFIX=/opt/sarus/default HOME=/home/docker \
        pytest -v -m 'not asroot' /sarus-source/CI/src/integration_tests/" # TIP: Add -s --last-failed to pytest to get more output from failed tests.
    fail_on_error "Python integration tests failed"
    echo "Successfully run integration tests with user=docker"

    echo "Running integration tests with user=root"
    sudo --login bash -c "
        PATH=/opt/sarus/default/bin:\$PATH PYTHONPATH=/sarus-source/CI/src:\$PYTHONPATH \
        CMAKE_INSTALL_PREFIX=/opt/sarus/default HOME=/home/docker \
        pytest -v -m asroot /sarus-source/CI/src/integration_tests/"
    fail_on_error "Python integration tests as root failed"
    echo "Successfully run integration tests with user=root"
}

run_gcov() {
    local build_dir=$1; shift || error "${FUNCNAME}: missing build_dir argument"

    sudo -u docker mkdir ${build_dir}/gcov
    cd ${build_dir}/gcov
    sudo -u docker gcov --preserve-paths $(find ${build_dir}/src -name "*.gcno" |grep -v test |tr '\n' ' ')
    fail_on_error "Failed to run gcov"
    sudo -u docker gcovr -r /sarus-source/src -k -g --object-directory ${build_dir}/gcov
    fail_on_error "Failed to run gcovr"
}

run_distributed_tests() {
    # Needs docker-compose, so usually runs on host
    local sarus_source_dir_on_host=$1; shift || error "${FUNCNAME}: missing sarus_source_dir_on_host argument"
    local sarus_archive=$1; shift || error "${FUNCNAME}: missing sarus_archive argument"
    local cache_oci_hooks_dir=$1; shift || error "${FUNCNAME}: missing cache_oci_hooks_dir argument"
    local cache_local_repo_dir=$1; shift || error "${FUNCNAME}: missing cache_local_repo_dir argument"
    local container_image=$1; shift || error "${FUNCNAME}: missing container_image argument"

    ${sarus_source_dir_on_host}/CI/run_integration_tests_for_virtual_cluster.sh ${sarus_archive} ${cache_oci_hooks_dir} ${cache_local_repo_dir} ${container_image}
    fail_on_error "Distributed tests in virtual cluster failed"
}

run_smoke_tests() {
    sarus --version
    fail_on_error "Failed to call Sarus"

    sarus pull alpine
    fail_on_error "Failed to pull image"

    sarus images
    fail_on_error "Failed to list images"

    sarus run alpine cat /etc/os-release > /tmp/sarus.out
    if [ "$(head -n 1 /tmp/sarus.out)" != "NAME=\"Alpine Linux\"" ]; then
        echo "Failed to run container"
        exit 1
    fi
    rm /tmp/sarus.out
}
