# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import sys
import tempfile
import re
import json

import common.util as util


class TestMPIHook(unittest.TestCase):
    """
    These tests verify that the host MPI libraries are properly brought into the container.
    """
    _SARUS_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"

    _SITE_LIBS_PREFIX = tempfile.mkdtemp()

    _HOST_MPI_LIBS = { "libmpi.so.12.5.5", "libmpich.so.12.5.5" }
    _HOST_MPI_DEPENDENCY_LIBS = { "libdependency0.so", "libdependency1.so" }

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._create_site_resources()
        cls._modify_bundle_config()

    @classmethod
    def tearDownClass(cls):
        cls._undo_create_site_resources()
        cls._undo_modify_bundle_config()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="ethcscs/dockerfiles:sarus_mpi_support_test-mpich_compatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="ethcscs/dockerfiles:sarus_mpi_support_test-mpich_incompatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="ethcscs/dockerfiles:sarus_mpi_support_test-no_mpi_libraries")

    @classmethod
    def _create_site_resources(cls):
        ci_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        dummy_lib_path = ci_dir + "/dummy_libs/lib_dummy_0.so"
        # MPI libs
        for value in cls._HOST_MPI_LIBS:
            lib_path = cls._SITE_LIBS_PREFIX + "/" + value
            subprocess.call(["cp", dummy_lib_path, lib_path])
        # MPI dependency libs
        for value in cls._HOST_MPI_DEPENDENCY_LIBS:
            lib_path = cls._SITE_LIBS_PREFIX + "/" + value
            subprocess.call(["cp", dummy_lib_path, lib_path])

    @classmethod
    def _undo_create_site_resources(cls):
        subprocess.call(["rm", "-r", cls._SITE_LIBS_PREFIX])

    @classmethod
    def _modify_bundle_config(cls):
        # make backup of config file
        subprocess.call(["sudo", "cp", cls._SARUS_CONFIG_FILE, cls._SARUS_CONFIG_FILE + ".bak"])

        # Build OCIHooks object
        mpi_hook = dict()
        mpi_hook["path"] = os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/mpi_hook"
        mpi_hook["env"] = list()
        libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_LIBS]
        mpi_hook["env"].append("SARUS_MPI_LIBS=" + ":".join(libs))
        libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_DEPENDENCY_LIBS]
        mpi_hook["env"].append("SARUS_MPI_DEPENDENCY_LIBS=" + ":".join(libs))

        hooks = dict()
        hooks["prestart"] = [mpi_hook]
        
        # Modify config file
        with open(cls._SARUS_CONFIG_FILE, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["OCIHooks"] = hooks
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", cls._SARUS_CONFIG_FILE])
        os.remove("sarus.json.dummy")

    @classmethod
    def _undo_modify_bundle_config(cls):
        subprocess.call(["sudo", "mv", cls._SARUS_CONFIG_FILE + ".bak", cls._SARUS_CONFIG_FILE])

    def setUp(self):
        self._mpi_command_line_option = None

    def test_no_mpi_support(self):
        self._mpi_command_line_option = False
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-mpich_compatible"
        libs = self._get_host_libs_in_container()
        assert not libs.issuperset(self._HOST_MPI_LIBS)
        assert not libs.issuperset(self._HOST_MPI_DEPENDENCY_LIBS)

    def test_mpich_compatible(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-mpich_compatible"
        libs = self._get_host_libs_in_container()
        assert libs.issuperset(self._HOST_MPI_LIBS)
        assert libs.issuperset(self._HOST_MPI_DEPENDENCY_LIBS)

    def test_mpich_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-mpich_incompatible"
        self._assert_sarus_raises_mpi_error_containing_text(
            text = "not ABI compatible with container's library")

    def test_container_without_mpi_libraries(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-no_mpi_libraries"
        self._assert_sarus_raises_mpi_error_containing_text(
            text = "No MPI libraries found in the container")

    def _get_host_libs_in_container(self):
        libs = set()
        output = self._get_command_output_in_container(["mount"])
        for line in output:
            if re.search(r".* on .*lib.*\.so(\.[0-9]+)* .*", line):
                lib = re.sub(r".* on .*(lib.*\.so(\.[0-9]+)*) .*", r"\1", line)
                libs.add(lib)
        return libs

    def _get_command_output_in_container(self, command):
        options = []
        if self._mpi_command_line_option:
            options.append("--mpi")
        return util.run_command_in_container(is_centralized_repository=False,
                                                    image=self._container_image,
                                                    command=command,
                                                    options_of_run_command=options)

    def _assert_sarus_raises_mpi_error_containing_text(self, text):
        if text in self._get_sarus_error_output():
            return
        raise Exception("Sarus didn't generate an MPI error containing the text \"{}\", "
            "but one was expected.".format(text))

    def _get_sarus_error_output(self):
        command = ["sarus", "run", "--mpi", self._container_image, "true"]
        try:
            subprocess.check_output(command, stderr=subprocess.STDOUT)
            raise Exception("Sarus didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            return ex.output

    def _get_command_output_in_container(self, command):
        options = []
        if self._mpi_command_line_option:
            options.append("--mpi")
        return util.run_command_in_container(is_centralized_repository=False,
                                                    image=self._container_image,
                                                    command=command,
                                                    options_of_run_command=options)
