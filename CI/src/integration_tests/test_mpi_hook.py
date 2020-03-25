# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import tempfile
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

    _CI_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    _DUMMY_LIB_PATH = _CI_DIR + "/dummy_libs/lib_dummy_0.so"
    _HOST_LIB_HASH = subprocess.check_output(["md5sum", _DUMMY_LIB_PATH]).decode().split()[0]

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
                                     image="ethcscs/dockerfiles:sarus_mpi_support_test-mpich_major_incompatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="ethcscs/dockerfiles:sarus_mpi_support_test-mpich_minor_incompatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="ethcscs/dockerfiles:sarus_mpi_support_test-no_mpi_libraries")

    @classmethod
    def _create_site_resources(cls):
        # MPI libs
        for value in cls._HOST_MPI_LIBS:
            lib_path = cls._SITE_LIBS_PREFIX + "/" + value
            subprocess.call(["cp", cls._DUMMY_LIB_PATH, lib_path])
        # MPI dependency libs
        for value in cls._HOST_MPI_DEPENDENCY_LIBS:
            lib_path = cls._SITE_LIBS_PREFIX + "/" + value
            subprocess.call(["cp", cls._DUMMY_LIB_PATH, lib_path])

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
        mpi_hook["env"].append("LDCONFIG_PATH=ldconfig")
        libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_LIBS]
        mpi_hook["env"].append("MPI_LIBS=" + ":".join(libs))
        libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_DEPENDENCY_LIBS]
        mpi_hook["env"].append("MPI_DEPENDENCY_LIBS=" + ":".join(libs))

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
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def test_mpich_compatible(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-mpich_compatible"
        hashes = self._get_hashes_of_host_libs_in_container()
        number_of_expected_mounts = len(self._HOST_MPI_LIBS) + len(self._HOST_MPI_DEPENDENCY_LIBS)
        assert hashes.count(self._HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_minor_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-mpich_minor_incompatible"
        self._assert_sarus_raises_mpi_warning_containing_text(
            text = "Partial ABI compatibility detected", expected_occurrences=2)
        hashes = self._get_hashes_of_host_libs_in_container()
        number_of_expected_mounts = len(self._HOST_MPI_LIBS) + len(self._HOST_MPI_DEPENDENCY_LIBS)
        assert hashes.count(self._HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_major_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-mpich_major_incompatible"
        self._assert_sarus_raises_mpi_error_containing_text(
            text = "not ABI compatible with container's MPI library")

    def test_container_without_mpi_libraries(self):
        self._mpi_command_line_option = True
        self._container_image = "ethcscs/dockerfiles:sarus_mpi_support_test-no_mpi_libraries"
        self._assert_sarus_raises_mpi_error_containing_text(
            text = "No MPI libraries found in the container")

    def _get_hashes_of_host_libs_in_container(self):
        options = []
        if self._mpi_command_line_option:
            options.append("--mpi")
        hashes = util.get_hashes_of_host_libs_in_container(is_centralized_repository=False,
                                                           image=self._container_image,
                                                           options_of_run_command=options)
        return hashes

    def _assert_sarus_raises_mpi_error_containing_text(self, text):
        if text in self._get_sarus_error_output():
            return
        raise Exception("Sarus didn't generate an MPI error containing the text \"{}\", "
            "but one was expected.".format(text))

    def _assert_sarus_raises_mpi_warning_containing_text(self, text, expected_occurrences):
        output = self._get_sarus_warn_output()
        if sum(["[WARN]" in line and text in line for line in output]) == expected_occurrences:
            return
        raise Exception("Sarus didn't generate an MPI warning containing the text \"{}\", "
            "but one was expected.".format(text))

    def _get_sarus_error_output(self):
        command = ["sarus", "run", "--mpi", self._container_image, "true"]
        try:
            subprocess.check_output(command, stderr=subprocess.STDOUT)
            raise Exception("Sarus didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            return ex.output.decode()

    def _get_sarus_warn_output(self):
        command = ["sarus", "run", "--mpi", self._container_image, "true"]
        out = subprocess.check_output(command, stderr=subprocess.STDOUT).decode()
        return util.command_output_without_trailing_new_lines(out)
