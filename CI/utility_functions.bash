#! /bin/echo This file is meant to be sourced

fail_on_error() {
    local last_command_exit_code=$?
    local message=$1
    if [ ${last_command_exit_code} -ne 0 ]; then
        echo "${message}"
        exit 1
    fi
}

change_uid_gid_of_docker_user() {
    local host_uid=$1
    local host_gid=$2

    echo "Changing UID/GID of Docker user"
    sudo usermod -u $host_uid docker
    sudo groupmod -g $host_gid docker
    sudo find / -path /proc -prune -o -user docker -exec chown -h $host_uid {} \;
    sudo find / -path /proc -prune -o -group docker -exec chgrp -h $host_gid {} \;
    sudo usermod -g $host_gid docker
    sudo usermod -d /home/docker docker
    echo "Successfully changed UID/GID of Docker user"
}

build_sarus_archive() {
    # Build and package Sarus as a standalone archive ready for installation
    local build_type=$1; shift
    local build_dir=$1; shift

    # Enable code coverage only with debug build
    if [ "$build_type" = "Debug" ]; then
        local cmake_toolchain_file=../cmake/toolchain_files/gcc-gcov.cmake
    else
        local cmake_toolchain_file=../cmake/toolchain_files/gcc.cmake
    fi

    # Build Sarus
    echo "Building Sarus (build type = ${build_type})"
    mkdir -p ${build_dir} && cd ${build_dir}
    local prefix_dir=${build_dir}/install/$(git describe --tags --dirty)-${build_type}
    cmake   -DCMAKE_TOOLCHAIN_FILE=${cmake_toolchain_file} \
            -DCMAKE_PREFIX_PATH="/opt/boost/1_65_0;/opt/cpprestsdk/v2.10.0;/opt/libarchive/3.4.1;/opt/rapidjson/rapidjson-master" \
            -Dcpprestsdk_INCLUDE_DIR=/opt/cpprestsdk/v2.10.0/include \
            -DCMAKE_BUILD_TYPE=${build_type} \
            -DBUILD_STATIC=TRUE \
            -DENABLE_TESTS_WITH_VALGRIND=FALSE \
            -DENABLE_SSH=TRUE \
            -DCMAKE_INSTALL_PREFIX=${prefix_dir} \
            ..
    make -j$(nproc)
    echo "Successfully built Sarus"

    # Build archive
    local archive_name="sarus-${build_type}.tar.gz"
    echo "Building archive ${archive_name}"
    rm -rf ${prefix_dir}/*
    make install
    rsync -arvL --chmod=go-w ${build_dir}/../standalone/ ${prefix_dir}/
    mkdir -p ${prefix_dir}/var/OCIBundleDir

    # Bring cached binaries if available (see Dockerfile.build)
    cp /usr/local/bin/tini-static-amd64 ${prefix_dir}/bin || true
    cp /usr/local/bin/runc.amd64 ${prefix_dir}/bin || true

    # Tar archive
    cd ${prefix_dir}/.. && tar cz --owner=root --group=root --file=../${archive_name} *
    cp  ${build_dir}/${archive_name} ${build_dir}/../${archive_name}

    # Add standalone's README at the artifacts level
    if [ "${CI}" ] || [ "${TRAVIS}" ]; then
        # Standalone README goes to root directory to be used by CI as root-level deployment artifact
        # This way users can read extracting instruction before actually extracting the standalone archive :)
        cp  ${build_dir}/../standalone/README.md ${build_dir}/../README.md
    fi
    echo "Successfully built archive"
}

install_sarus_from_archive() {
    local root_dir=$1; shift
    local archive=$1; shift
    local pwd_backup=$(pwd)

    echo "Installing Sarus from archive ${archive}"

    mkdir -p ${root_dir}
    fail_on_error "Failed to install Sarus"
    cd ${root_dir}
    rm -rf * #remove any other installation
    tar xf ${archive}
    fail_on_error "Failed to install Sarus"
    local sarus_version=$(ls)
    ln -s ${sarus_version} default
    fail_on_error "Failed to install Sarus"
    ./default/configure_installation.sh
    fail_on_error "Failed to install Sarus"

    export PATH=/opt/sarus/default/bin:${PATH}
    echo "Successfully installed Sarus"

    cd ${pwd_backup}
}

run_unit_tests() {
    local build_dir=$1; shift
    cd ${build_dir}

    sudo -u docker CTEST_OUTPUT_ON_FAILURE=1 ctest --exclude-regex AsRoot
    fail_on_error "Unit tests as normal user failed"

    CTEST_OUTPUT_ON_FAILURE=1 ctest --tests-regex AsRoot
    fail_on_error "Unit tests as root user failed"

    # Adjust ownership of coverage files (Debug only)
    # after execution of unit tests as root
    find ${build_dir} -name "*.gcda" -exec chown docker:docker {} \;
    fail_on_error "Failed to chown *.gcda files (necessary to update code coverage as non-root user)"
}

run_integration_tests() {
    local build_dir=$1; shift
    local build_type=$1; shift

    install_sarus_from_archive /opt/sarus /sarus-source/build-${build_type}/sarus-${build_type}.tar.gz
    fail_on_error "Failed to install Sarus from archive"

    sudo -u docker PATH=/opt/sarus/default/bin:$PATH PYTHONPATH=/sarus-source/CI/src:$PYTHONPATH CMAKE_INSTALL_PREFIX=/opt/sarus/default HOME=/home/docker pytest -v -m 'not asroot' /sarus-source/CI/src/integration_tests/
    fail_on_error "Python integration tests failed"

    sudo PATH=/opt/sarus/default/bin:$PATH PYTHONPATH=/sarus-source/CI/src:$PYTHONPATH CMAKE_INSTALL_PREFIX=/opt/sarus/default HOME=/home/docker pytest -v -m asroot /sarus-source/CI/src/integration_tests/
    fail_on_error "Python integration tests as root failed"

    # Run coverage only on Debug build of GitLab CI
    if [ ${CI} ] && [ ${build_type} = "Debug" ]; then
        sudo -u docker mkdir ${build_dir}/gcov
        cd ${build_dir}/gcov
        sudo -u docker gcov --preserve-paths $(find ${build_dir}/src -name "*.gcno" |grep -v test |tr '\n' ' ')
        fail_on_error "Failed to run gcov"
        sudo -u docker gcovr -r /sarus-source/src -k -g --object-directory ${build_dir}/gcov
        fail_on_error "Failed to run gcovr"
    fi
}

run_distributed_tests() {
    # Needs docker-compose, so usually runs on host
    local sarus_source_dir_on_host=$1; shift
    local build_type=$1; shift
    local cache_dir=$1; shift

    ${sarus_source_dir_on_host}/CI/run_integration_tests_for_virtual_cluster.sh /sarus-source/build-${build_type}/sarus-${build_type}.tar.gz ${cache_dir}
    fail_on_error "Distributed tests in virtual cluster failed"
}

run_smoke_tests() {
    sarus --version
    fail_on_error "Failed to call Sarus"

    sarus pull alpine
    fail_on_error "Failed to pull image"

    sarus images
    fail_on_error "Failed to list images"

    sarus run alpine cat /etc/os-release > sarus.out
    if [ "$(head -n 1 sarus.out)" != "NAME=\"Alpine Linux\"" ]; then
        echo "Failed to run container"
        exit 1
    fi
}
