#!/bin/bash
set -ex

base_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

pip3 install -r ${base_dir}/requirements_tests.txt
${base_dir}/install_dep_boost.bash
${base_dir}/install_dep_rapidjson.bash
${base_dir}/install_dep_runc.bash
${base_dir}/install_dep_tini.bash
${base_dir}/install_dep_skopeo.bash
${base_dir}/install_dep_umoci.bash

# Note Dropbear and cpputest are also dependencies, for now managed by cmake.
