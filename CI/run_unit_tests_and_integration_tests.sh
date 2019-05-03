#!/bin/bash

set -ex

cd $(dirname "$0")

sarus_prefixdir=$1; shift
build_type=$1; shift
host_uid=$1; shift
host_gid=$1; shift
sarus_src_dir=/sarus-source
build_dir=$sarus_src_dir/build

. utility_functions.bash
change_uid_gid_of_docker_user $host_uid $host_gid
add_supplementary_groups_to_docker_user "test1" "test2"

# install sarus without security checks for executing unit tests
sudo -u docker bash -c ". utility_functions.bash && install_sarus $sarus_prefixdir $build_type FALSE"

# run tests with an invalid locale (some systems/user accounts have indeed
# invalid locale settings and Sarus used to fail in such cases)
export LC_CTYPE=invalid

# unit tests
cd $build_dir
sudo -u docker CTEST_OUTPUT_ON_FAILURE=1 ctest --exclude-regex 'AsRoot'
CTEST_OUTPUT_ON_FAILURE=1 ctest --tests-regex 'AsRoot'

# unit test coverage (Debug build only)
if [ "$build_type" = "Debug" ]; then
    gcovr -r $sarus_src_dir/src -k -g --object-directory $build_dir/src
fi

# re-install sarus with security checks for executing integration tests
cd $sarus_src_dir/CI/
sudo -u docker bash -c ". utility_functions.bash && install_sarus $sarus_prefixdir $build_type TRUE"

# integration tests
cd $sarus_src_dir/CI/src
sudo -u docker PYTHONPATH=$(pwd):$PYTHONPATH PATH=$sarus_prefixdir/bin:$PATH CMAKE_INSTALL_PREFIX=$sarus_prefixdir HOME=/home/docker nosetests -v integration_tests/test*.py
