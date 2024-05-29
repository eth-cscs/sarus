# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import shutil
import tempfile
import pytest

import common.util as util


class TestMPIHook(unittest.TestCase):
    """
    These tests verify that the host MPI libraries are properly brought into the container.
    """
    _OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/mpi_hook.json"

    _SITE_LIBS_PREFIX = tempfile.mkdtemp()

    _HOST_MPI_LIBS = {"libmpi.so.12.5.5", "libmpich.so.12.5.5"}
    _HOST_MPI_DEPENDENCY_LIBS = {"libdependency0.so", "libdependency1.so"}

    _CI_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    _DUMMY_LIB_PATH = _CI_DIR + "/dummy_libs/lib_dummy_0.so"
    _HOST_LIB_HASH = util.generate_file_md5_hash(_DUMMY_LIB_PATH, "md5")

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._create_site_resources()
        cls._enable_hook(cls._generate_hook_config())

    @classmethod
    def tearDownClass(cls):
        cls._remove_site_resources()
        cls._disable_hook()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:mpich_compatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:mpich_compatible_symlink")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:mpich_major_incompatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:mpich_minor_incompatible")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:no_mpi_libraries")
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry_f35")

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
    def _remove_site_resources(cls):
        shutil.rmtree(cls._SITE_LIBS_PREFIX)

    @classmethod
    def _enable_hook(cls, hook_config):
        util.create_hook_file(hook_config, cls._OCIHOOK_CONFIG_FILE)

    @classmethod
    def _disable_hook(cls):
        subprocess.call(["sudo", "rm", cls._OCIHOOK_CONFIG_FILE])

    @classmethod
    def _generate_hook_config(cls):
        mpi_libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_LIBS]
        mpi_dependency_libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_DEPENDENCY_LIBS]

        hook_config = {
            "version": "1.0.0",
            "hook": {
                "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/mpi_hook",
                "env": [
                    "LDCONFIG_PATH=" + shutil.which("ldconfig"),
                    "MPI_LIBS=" + ":".join(mpi_libs),
                    "MPI_DEPENDENCY_LIBS=" + ":".join(mpi_dependency_libs),
                ]
            },
            "when": {
                "annotations": {
                    "^com.hooks.mpi.enabled$": "^true$"
                }
            },
            "stages": ["createContainer"]
        }

        return hook_config

    def setUp(self):
        self._mpi_command_line_option = None

    def test_no_mpi_support(self):
        self._mpi_command_line_option = False
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_compatible"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def test_mpich_compatible(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_compatible"
        hashes = self._get_hashes_of_host_libs_in_container()
        number_of_expected_mounts = len(self._HOST_MPI_LIBS) + len(self._HOST_MPI_DEPENDENCY_LIBS)
        assert hashes.count(self._HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_compatible_symlink(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_compatible_symlink"
        hashes = self._get_hashes_of_host_libs_in_container()
        number_of_expected_mounts = len(self._HOST_MPI_LIBS) + len(self._HOST_MPI_DEPENDENCY_LIBS)
        assert hashes.count(self._HOST_LIB_HASH) == number_of_expected_mounts

    @pytest.mark.xfail(reason="Hooks stdout/err are not captured by Pytest after changes for runc 1.1.12")
    def test_mpich_minor_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_minor_incompatible"
        self._assert_sarus_raises_mpi_warning_containing_text(
            text="Partial ABI compatibility detected", expected_occurrences=2)
        hashes = self._get_hashes_of_host_libs_in_container()
        number_of_expected_mounts = len(self._HOST_MPI_LIBS) + len(self._HOST_MPI_DEPENDENCY_LIBS)
        assert hashes.count(self._HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_major_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_major_incompatible"
        self._assert_sarus_raises_mpi_error_containing_text("not ABI compatible with container's MPI library")

    def test_container_without_mpi_libraries(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:no_mpi_libraries"
        self._assert_sarus_raises_mpi_error_containing_text("No MPI libraries found in the container")

    def test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry_f35"
        self._assert_sarus_raises_mpi_error_containing_text("No MPI libraries found in the container")

    def _get_hashes_of_host_libs_in_container(self):
        options = []
        if self._mpi_command_line_option:
            options.append("--mpi")
        hashes = util.get_hashes_of_host_libs_in_container(is_centralized_repository=False,
                                                           image=self._container_image,
                                                           options_of_run_command=options)
        return hashes

    def _assert_sarus_raises_mpi_error_containing_text(self, text):
        command = ["sarus", "run", "--mpi", self._container_image, "true"]
        util.assert_sarus_raises_error_containing_text(command, text)

    def _assert_sarus_raises_mpi_warning_containing_text(self, text, expected_occurrences):
        command = ["sarus", "run", "--mpi", self._container_image, "true"]
        output = util.get_sarus_error_output(command, fail_expected=False)
        number_of_occurrences = sum(["[WARN]" in line and text in line for line in output.split('\n')])
        assert number_of_occurrences == expected_occurrences, 'Sarus didn\'t generate the expected MPI warnings containing the text "{}".'.format(text)


@pytest.mark.asroot
class TestMPIHookDevices(unittest.TestCase):
    """
    These tests verify that the MPI hook is able to mount and whitelist devices
    for rw access in the container devices cgroup.
    """
    DEVICE_FILENAME = "/dev/test0"
    CONTAINER_IMAGE = "quay.io/ethcscs/sarus-integration-tests:mpich_compatible"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        TestMPIHook._create_site_resources()
        cls._create_device_file()
        TestMPIHook._enable_hook(cls._generate_hook_config_with_device())

    @classmethod
    def tearDownClass(cls):
        TestMPIHook.tearDownClass()
        os.remove(cls.DEVICE_FILENAME)

    @classmethod
    def _create_device_file(cls):
        import stat
        device_mode = 0o666 | stat.S_IFCHR
        device_id = os.makedev(511, 511)
        os.mknod(cls.DEVICE_FILENAME, device_mode, device_id)

    @classmethod
    def _generate_hook_config_with_device(cls):
        hook_config = TestMPIHook._generate_hook_config()
        hook_config["hook"]["env"].append("BIND_MOUNTS=/dev/test0:/var/opt:/var/lib")
        return hook_config

    def test_whitelist_device(self):
        devices_list = self._get_devices_list_from_cgroup_in_container()
        assert "c 511:511 rw" in devices_list

    def _get_devices_list_from_cgroup_in_container(self):
        return util.run_command_in_container(is_centralized_repository=False,
                                             image=self.CONTAINER_IMAGE,
                                             command=["cat", "/sys/fs/cgroup/devices/devices.list"],
                                             options_of_run_command=["--mpi"])
