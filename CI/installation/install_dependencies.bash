#!/bin/bash

# Note OpenSSH and cpputest are also dependencies, for now managed by cmake.

base_dir=$(dirname -- $0)
sudo bash ${base_dir}/install_dep_boost.bash
sudo bash ${base_dir}/install_dep_libarchive.bash
sudo bash ${base_dir}/install_dep_cpprestsdk.bash
sudo bash ${base_dir}/install_dep_rapidjson.bash
sudo bash ${base_dir}/install_dep_runc.bash
sudo bash ${base_dir}/install_dep_tini.bash
