# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import shutil
import tempfile
import json
import pytest

import common.util as util


class TestMPIHook(unittest.TestCase):
    """
    These tests verify that the host MPI libraries are properly brought into the container.
    """
    _OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/mpi_hook.json"

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
        cls._enable_hook()

    @classmethod
    def tearDownClass(cls):
        cls._undo_create_site_resources()
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
                                     image="quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry")

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
    def _enable_hook(cls):
        mpi_libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_LIBS]
        mpi_dependency_libs = [cls._SITE_LIBS_PREFIX + "/" + value for value in cls._HOST_MPI_DEPENDENCY_LIBS]

        # create OCI hook's config file
        hook = dict()
        hook["version"] = "1.0.0"
        hook["hook"] = dict()
        hook["hook"]["path"] = os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/mpi_hook"
        hook["hook"]["env"] = [
            "LDCONFIG_PATH=" + shutil.which("ldconfig"),
            "MPI_LIBS=" + ":".join(mpi_libs),
            "MPI_DEPENDENCY_LIBS=" + ":".join(mpi_dependency_libs),
        ]

        hook["when"] = dict()
        hook["when"]["annotations"] = {"^com.hooks.mpi.enabled$": "^true$"}
        hook["stages"] = ["prestart"]

        with open("hook_config.json.tmp", 'w') as f:
            f.write(json.dumps(hook))

        subprocess.check_output(["sudo", "mv", "hook_config.json.tmp", cls._OCIHOOK_CONFIG_FILE])
        subprocess.check_output(["sudo", "chown", "root:root", cls._OCIHOOK_CONFIG_FILE])
        subprocess.check_output(["sudo", "chmod", "644", cls._OCIHOOK_CONFIG_FILE])

    @classmethod
    def _disable_hook(cls):
        subprocess.call(["sudo", "rm", cls._OCIHOOK_CONFIG_FILE])

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

    def test_mpich_minor_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_minor_incompatible"
        self._assert_sarus_raises_mpi_warning_containing_text(
            text = "Partial ABI compatibility detected", expected_occurrences=2)
        hashes = self._get_hashes_of_host_libs_in_container()
        number_of_expected_mounts = len(self._HOST_MPI_LIBS) + len(self._HOST_MPI_DEPENDENCY_LIBS)
        assert hashes.count(self._HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_major_incompatible(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:mpich_major_incompatible"
        self._assert_sarus_raises_mpi_error_containing_text(
            text = "not ABI compatible with container's MPI library")

    def test_container_without_mpi_libraries(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:no_mpi_libraries"
        self._assert_sarus_raises_mpi_error_containing_text(
            text = "No MPI libraries found in the container")

    def test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(self):
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry"
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
        assert text in self._get_sarus_error_output(), 'Sarus didn\'t generate an MPI error containing the text "{}", but one was expected.'.format(text)

    def _assert_sarus_raises_mpi_warning_containing_text(self, text, expected_occurrences):
        output = self._get_sarus_warn_output()
        number_of_occurrences = sum(["[WARN]" in line and text in line for line in output])
        assert number_of_occurrences == expected_occurrences, 'Sarus didn\'t generate the expected MPI warnings containing the text "{}".'.format(text)

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


@pytest.mark.asroot
class TestMPIHookDevices(unittest.TestCase):
    """
    These tests verify that the MPI hook is able to mount and whitelist devices
    for rw access in the container devices cgroup.
    """
    DEVICE_FILENAME = "/dev/test0"

    @classmethod
    def setUpClass(cls):
        TestMPIHook.setUpClass()
        cls._create_device_file()
        cls._add_device_to_hook_config()

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
    def _add_device_to_hook_config(cls):
        with open(TestMPIHook._OCIHOOK_CONFIG_FILE) as json_file:
            hook = json.load(json_file)

        hook["hook"]["env"].append("BIND_MOUNTS=/dev/test0:/var/opt:/var/lib")

        with open("hook_config.json.tmp", 'w') as f:
            f.write(json.dumps(hook))

        subprocess.check_output(["sudo", "mv", "hook_config.json.tmp", TestMPIHook._OCIHOOK_CONFIG_FILE])
        subprocess.check_output(["sudo", "chown", "root:root", TestMPIHook._OCIHOOK_CONFIG_FILE])
        subprocess.check_output(["sudo", "chmod", "644", TestMPIHook._OCIHOOK_CONFIG_FILE])

    def test_whitelist_device(self):
        devices_list = self._get_devices_list_from_cgroup_in_container()
        assert "c 511:511 rw" in devices_list

    def _get_devices_list_from_cgroup_in_container(self):
        return util.run_command_in_container(is_centralized_repository=False,
                                             image="quay.io/ethcscs/sarus-integration-tests:mpich_compatible",
                                             command=["cat", "/sys/fs/cgroup/devices/devices.list"],
                                             options_of_run_command=["--mpi"])
