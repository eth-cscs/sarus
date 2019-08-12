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

add_supplementary_groups_to_docker_user() {
    for group in "$@"; do
        sudo groupadd $group
        sudo usermod -a -G $group docker
    done
}

# build and package Sarus as a standalone archive ready for installation
build_sarus_archive() {
    local build_type=$1; shift
    local enable_security_checks=$1; shift
    local pwd_backup=$(pwd)

    local enable_coverage=FALSE
    if [ "$build_type" = "Debug" ]; then
        enable_coverage=TRUE
    fi

    # build Sarus
    echo "Building Sarus (build type = ${build_type}, security checks = ${enable_security_checks})"
    local build_dir=/sarus-source/build-${build_type}
    mkdir -p ${build_dir} && cd ${build_dir}
    local prefix_dir=${build_dir}/install/$(git describe --tags --dirty)-${build_type}
    cmake   -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_files/gcc-gcov.cmake \
            -DCMAKE_PREFIX_PATH="/opt/boost/1_65_0;/opt/cpprestsdk/v2.10.0;/opt/libarchive/3.3.1;/opt/rapidjson/rapidjson-master" \
            -Dcpprestsdk_INCLUDE_DIR=/opt/cpprestsdk/v2.10.0/include \
            -DCMAKE_BUILD_TYPE=${build_type} \
            -DBUILD_STATIC=TRUE \
            -DENABLE_RUNTIME_SECURITY_CHECKS=${enable_security_checks} \
            -DENABLE_TESTS_WITH_GCOV=${enable_coverage} \
            -DENABLE_TESTS_WITH_VALGRIND=FALSE \
            -DENABLE_SSH=TRUE \
            -DCMAKE_INSTALL_PREFIX=${prefix_dir} \
            ..
    make -j$(nproc)
    echo "Successfully built Sarus"

    # build archive
    local archive_name="sarus.tar.gz"
    echo "Building archive ${archive_name}"
    make install
    (cd ${prefix_dir}/bin && wget https://github.com/opencontainers/runc/releases/download/v1.0.0-rc8/runc.amd64 && chmod +x runc.amd64)
    (cd ${prefix_dir} && mkdir -p var/OCIBundleDir)
    (cd ${prefix_dir}/.. && tar cfz ../${archive_name} *)
    echo "Successfully built archive"

    cd ${pwd_backup}
}

install_sarus_from_archive() {
    local root_dir=$1; shift
    local archive=$1; shift
    local pwd_backup=$(pwd)

    echo "Installing Sarus from archive ${archive}"
    mkdir -p ${root_dir}
    cd ${root_dir}
    rm -rf * #remove any other installation
    tar xf ${archive}
    local sarus_version=$(ls)
    ln -s ${sarus_version} default
    ./default/configure_installation.sh
    echo "Successfully installed Sarus"

    cd ${pwd_backup}
}

run_tests() {
    local host_uid=$1; shift
    local host_gid=$1; shift
    local build_type=$1; shift
    local security_checks=$1; shift
    local build_dir=$1; shift

    change_uid_gid_of_docker_user ${host_uid} ${host_gid}
    cd ${build_dir}

    sudo -u docker CTEST_OUTPUT_ON_FAILURE=1 ctest --exclude-regex AsRoot
    fail_on_error "Unit tests as normal user failed"

    CTEST_OUTPUT_ON_FAILURE=1 ctest --tests-regex AsRoot
    fail_on_error "Unit tests as root user failed"

    sudo -u docker /sarus-source/CI/run_documentation_build_test.sh
    fail_on_error "Documentation build test failed"

    install_sarus_from_archive /opt/sarus ${build_dir}/sarus.tar.gz
    fail_on_error "Failed to install Sarus from archive"

    sudo -u docker PYTHONPATH=/sarus-source/CI/src:$PYTHONPATH PATH=/opt/sarus/default/bin:$PATH CMAKE_INSTALL_PREFIX=/opt/sarus/default HOME=/home/docker nosetests -v /sarus-source/CI/src/integration_tests/test*.py \
    fail_on_error "Python integration tests failed"

    if [ ${build_type} = "Debug" ]; then
        sudo -u docker mkdir ${build_dir}/gcov
        cd ${build_dir}/gcov
        sudo -u docker gcov --preserve-paths $(find ${build_dir}/src -name "*.gcno" |grep -v test |tr '\n' ' ')
        sudo -u docker gcovr -r /sarus-source/src -k -g --object-directory ${build_dir}/gcov
    fi
}

build_install_test_sarus() {
    local build_type=$1; shift
    local security_checks=$1; shift

    local host_uid=$(id -u)
    local host_gid=$(id -g)
    local docker_image_build=ethcscs/sarus-ci-build:1.0.0
    local docker_image_run=ethcscs/sarus-ci-run:1.0.0
    local build_folder=build-${build_type}
    local build_dir=/sarus-source/${build_folder}

    if [ ${security_checks} = TRUE ]; then
        build_folder=${build-folder}-WithSecurityChecks
    fi

    # Use cached build artifacts
    test -e ~/cache/ids/sarus/openssh.tar && mkdir -p ${build_folder}/dep && cp ~/cache/ids/sarus/openssh.tar ${build_folder}/dep/openssh.tar
    test -e ~/cache/ids/sarus/cpputest && mkdir -p ${build_folder}/dep && cp -r ~/cache/ids/sarus/cpputest ${build_folder}/dep/cpputest

    # Build Sarus archive
    docker run --tty --rm --user root --mount=src=$(pwd),dst=/sarus-source,type=bind ${docker_image_build} bash -c "
        . /sarus-source/CI/utility_functions.bash \
        && change_uid_gid_of_docker_user ${host_uid} ${host_gid} \
        && sudo -u docker bash -c '. /sarus-source/CI/utility_functions.bash && build_sarus_archive ${build_type} ${security_checks}'"

    # Run tests
    local sarus_cached_home_dir=~/cache/ids/sarus/home_dir
    local sarus_cached_centralized_repository_dir=~/cache/ids/sarus/centralized_repository_dir
    docker run --tty --rm --user root --privileged \
        --mount=src=$(pwd),dst=/sarus-source,type=bind \
        --mount=src=${sarus_cached_home_dir},dst=/home/docker,type=bind \
        --mount=src=${sarus_cached_centralized_repository_dir},dst=/var/sarus/centralized_repository,type=bind \
        ${docker_image_run} \
        bash -c ". /sarus-source/CI/utility_functions.bash && run_tests ${host_uid} ${host_gid} ${build_type} ${security_checks} ${build_dir}"
}

generate_slurm_conf() {
    slurm_conf_file=$1

    shift
    controller=$1

    shift
    servers=( "$@" )

    cat << EOF > $slurm_conf_file
#
# Example slurm.conf file. Please run configurator.html
# (in doc/html) to build a configuration file customized
# for your environment.
#
#
# slurm.conf file generated by configurator.html.
#
# See the slurm.conf man page for more information.
#
ClusterName=virtual-cluster
ControlMachine=$controller
SlurmUser=slurm
SlurmctldPort=6817
SlurmdPort=6818
StateSaveLocation=/var/lib/slurm-llnl
SlurmdSpoolDir=/tmp/slurmd
SwitchType=switch/none
MpiDefault=none
SlurmctldPidFile=/var/run/slurmctld.pid
SlurmdPidFile=/var/run/slurmd.pid
ProctrackType=proctrack/pgid
CacheGroups=0
ReturnToService=0
SlurmctldTimeout=300
SlurmdTimeout=300
InactiveLimit=0
MinJobAge=300
KillWait=30
Waittime=0
SchedulerType=sched/backfill
SelectType=select/linear
FastSchedule=1
# LOGGING
SlurmctldDebug=3
SlurmdDebug=3
JobCompType=jobcomp/none
EOF

    echo "# COMPUTE NODES" >> $slurm_conf_file
    for server in "${servers[@]}"; do
    echo "Nodename=$server" >> $slurm_conf_file
    done

    partition_nodes=
    for server in "${servers[@]}"; do
        if [ ! -z "$partition_nodes" ]; then
            partition_nodes=$partition_nodes,
        fi
        partition_nodes=$partition_nodes$server
    done
    echo "PartitionName=debug Nodes=$partition_nodes Default=YES MaxTime=INFINITE State=UP" >> $slurm_conf_file

    cat << EOF >> $slurm_conf_file
GresTypes=gpu
# settings required by SLURM plugin
PrologFlags=Alloc,Contain
EOF
}
