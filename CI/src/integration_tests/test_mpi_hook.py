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

_OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/mpi_hook.json"

_SITE_LIBS_PREFIX = tempfile.mkdtemp()

_HOST_MPI_LIBS_FULL = {"libmpi.so.12.5.5", "libmpich.so.12.5.5"}
_HOST_MPI_LIBS_MAJOR = {"libmpi.so.12", "libmpich.so.12"}
_HOST_MPI_DEPENDENCY_LIBS = {"libdependency0.so", "libdependency1.so"}

_CI_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_DUMMY_LIB_PATH = _CI_DIR + "/dummy_libs/lib_dummy_0.so"
_HOST_LIB_HASH = util.generate_file_md5_hash(_DUMMY_LIB_PATH, "md5")


def _generate_base_hook_config():
    mpi_dependency_libs = [_SITE_LIBS_PREFIX + "/" + value for value in _HOST_MPI_DEPENDENCY_LIBS]

    hook_config = {
        "version": "1.0.0",
        "hook": {
            "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/mpi_hook",
            "env": [
                "LDCONFIG_PATH=" + shutil.which("ldconfig"),
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


def _get_hook_config_and_libs_default():
    mpi_libs = _HOST_MPI_LIBS_MAJOR
    mpi_libs_paths = [_SITE_LIBS_PREFIX + "/" + value for value in mpi_libs]
    hook_config = _generate_base_hook_config()
    hook_config["hook"]["env"].append("MPI_LIBS=" + ":".join(mpi_libs_paths))
    return hook_config, mpi_libs


def _get_hook_config_and_libs_major():
    hook_config, mpi_libs = _get_hook_config_and_libs_default()
    hook_config["hook"]["env"].append("MPI_COMPATIBILITY_TYPE=major")
    return hook_config, mpi_libs


def _get_hook_config_and_libs_full():
    mpi_libs = _HOST_MPI_LIBS_FULL
    mpi_libs_paths = [_SITE_LIBS_PREFIX + "/" + value for value in mpi_libs]
    hook_config = _generate_base_hook_config()
    hook_config["hook"]["env"].append("MPI_LIBS=" + ":".join(mpi_libs_paths))
    hook_config["hook"]["env"].append("MPI_COMPATIBILITY_TYPE=full")
    return hook_config, mpi_libs


def _get_hook_config_and_libs_strict():
    hook_config, mpi_libs = _get_hook_config_and_libs_full()
    hook_config["hook"]["env"].append("MPI_COMPATIBILITY_TYPE=strict")
    return hook_config, mpi_libs


def assert_sarus_raises_mpi_warning_containing_text(container_image, text, expected_occurrences):
    command = ["sarus", "run", "--mpi", container_image, "true"]
    output = util.get_sarus_error_output(command, fail_expected=False)
    number_of_occurrences = sum(["[WARN]" in line and text in line for line in output.split('\n')])
    assert number_of_occurrences == expected_occurrences, \
        'Sarus didn\'t generate the expected MPI warnings containing the text "{}".'.format(text)


def assert_sarus_raises_mpi_error_containing_regex(container_image, text):
    command = ["sarus", "run", "--mpi", container_image, "true"]
    util.assert_sarus_raises_error_containing_regex(command, text)


def check_exception_message(hook_config, container_image, error_regex):
    with util.temporary_hook_files((hook_config, _OCIHOOK_CONFIG_FILE)):
        assert_sarus_raises_mpi_error_containing_regex(container_image=container_image, text=error_regex)


def get_hashes_of_host_libs_in_container(container_image, mpi_command_line_option=True):
    options = []
    if mpi_command_line_option:
        options.append("--mpi")
    hashes = util.get_hashes_of_host_libs_in_container(is_centralized_repository=False,
                                                       image=container_image,
                                                       options_of_run_command=options)
    return hashes


class TestMPIHook(unittest.TestCase):
    """
    These tests verify that the host MPI libraries are properly brought into the container.
    """

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._create_site_resources()

    @classmethod
    def tearDownClass(cls):
        cls._remove_site_resources()

    @classmethod
    def _pull_docker_images(cls):
        for tag in [
            "mpich_compatible", "mpich_compatible_symlink", "mpich_major_incompatible",
            "mpich_minor_incompatible", "no_mpi_libraries", "nonexisting_ldcache_entry_f35"
        ]:
            util.pull_image_if_necessary(is_centralized_repository=False, image=f"quay.io/ethcscs/sarus-integration-tests:{tag}")

    @classmethod
    def _create_site_resources(cls):
        # MPI libs
        for value in _HOST_MPI_LIBS_FULL | _HOST_MPI_LIBS_MAJOR:
            lib_path = _SITE_LIBS_PREFIX + "/" + value
            subprocess.call(["cp", _DUMMY_LIB_PATH, lib_path])
        # MPI dependency libs
        for value in _HOST_MPI_DEPENDENCY_LIBS:
            lib_path = _SITE_LIBS_PREFIX + "/" + value
            subprocess.call(["cp", _DUMMY_LIB_PATH, lib_path])

    @classmethod
    def _remove_site_resources(cls):
        shutil.rmtree(_SITE_LIBS_PREFIX)

    def test_no_mpi_support(self):
        hook_config, _ = _get_hook_config_and_libs_default()
        with util.temporary_hook_files((hook_config, _OCIHOOK_CONFIG_FILE)):
            hashes = get_hashes_of_host_libs_in_container(
                "quay.io/ethcscs/sarus-integration-tests:mpich_compatible",
                mpi_command_line_option=False
            )
            assert not hashes

    def test_mpich_compatible_default(self):
        self._test_mpich_compatible(*_get_hook_config_and_libs_default())

    def test_mpich_compatible_major(self):
        self._test_mpich_compatible(*_get_hook_config_and_libs_major())

    def test_mpich_compatible_full(self):
        self._test_mpich_compatible(*_get_hook_config_and_libs_full())

    def test_mpich_compatible_strict(self):
        self._test_mpich_compatible(*_get_hook_config_and_libs_strict())

    def _test_mpich_compatible(self, hook_config, mpi_libs):
        with util.temporary_hook_files((hook_config, _OCIHOOK_CONFIG_FILE)):
            hashes = get_hashes_of_host_libs_in_container("quay.io/ethcscs/sarus-integration-tests:mpich_compatible")
            number_of_expected_mounts = len(mpi_libs) + len(_HOST_MPI_DEPENDENCY_LIBS)
            assert hashes.count(_HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_compatible_symlink_default(self):
        self._test_mpich_compatible_symlink(*_get_hook_config_and_libs_default())

    def test_mpich_compatible_symlink_major(self):
        self._test_mpich_compatible_symlink(*_get_hook_config_and_libs_major())

    def test_mpich_compatible_symlink_full(self):
        self._test_mpich_compatible_symlink(*_get_hook_config_and_libs_full())

    def test_mpich_compatible_symlink_strict(self):
        self._test_mpich_compatible_symlink(*_get_hook_config_and_libs_strict())

    def _test_mpich_compatible_symlink(self, hook_config, mpi_libs):
        with util.temporary_hook_files((hook_config, _OCIHOOK_CONFIG_FILE)):
            hashes = get_hashes_of_host_libs_in_container("quay.io/ethcscs/sarus-integration-tests:mpich_compatible_symlink")
            number_of_expected_mounts = len(mpi_libs) + len(_HOST_MPI_DEPENDENCY_LIBS)
            assert hashes.count(_HOST_LIB_HASH) == number_of_expected_mounts

    def test_mpich_minor_incompatible_default(self):
        self._test_mpich_minor_incompatible(*_get_hook_config_and_libs_default(), 0, True)

    def test_mpich_minor_incompatible_major(self):
        self._test_mpich_minor_incompatible(*_get_hook_config_and_libs_major(), 0, True)

    @pytest.mark.xfail(reason="Hooks stdout/err are not captured by Pytest after changes for runc 1.1.12")
    def test_mpich_minor_incompatible_full(self):
        self._test_mpich_minor_incompatible(*_get_hook_config_and_libs_full(), 2, True)

    def test_mpich_minor_incompatible_strict(self):
        self._test_mpich_minor_incompatible(*_get_hook_config_and_libs_strict(), 0, False)

    def _test_mpich_minor_incompatible(self, hook_config, mpi_libs, num_warnings, success_expected):
        with util.temporary_hook_files((hook_config, _OCIHOOK_CONFIG_FILE)):
            if success_expected:
                assert_sarus_raises_mpi_warning_containing_text(
                    container_image="quay.io/ethcscs/sarus-integration-tests:mpich_minor_incompatible",
                    text="Partial ABI compatibility detected", expected_occurrences=num_warnings)
                hashes = get_hashes_of_host_libs_in_container("quay.io/ethcscs/sarus-integration-tests:mpich_minor_incompatible")
                number_of_expected_mounts = len(mpi_libs) + len(_HOST_MPI_DEPENDENCY_LIBS)
                assert hashes.count(_HOST_LIB_HASH) == number_of_expected_mounts
            else:
                assert_sarus_raises_mpi_error_containing_regex("quay.io/ethcscs/sarus-integration-tests:mpich_minor_incompatible", r"not[ \w]*ABI compatible with container's MPI library")

    def test_mpich_major_incompatible_default(self):
        self._test_mpich_major_incompatible(_get_hook_config_and_libs_default()[0])

    def test_mpich_major_incompatible_major(self):
        self._test_mpich_major_incompatible(_get_hook_config_and_libs_major()[0])

    def test_mpich_major_incompatible_full(self):
        self._test_mpich_major_incompatible(_get_hook_config_and_libs_full()[0])

    def test_mpich_major_incompatible_strict(self):
        self._test_mpich_major_incompatible(_get_hook_config_and_libs_strict()[0])

    def _test_mpich_major_incompatible(self, hook_config):
        check_exception_message(
            hook_config=hook_config,
            container_image="quay.io/ethcscs/sarus-integration-tests:mpich_major_incompatible",
            error_regex=r"not[ \w]*ABI compatible with container's MPI library"
        )

    def test_container_without_mpi_libraries_default(self):
        self._test_container_without_mpi_libraries(_get_hook_config_and_libs_default()[0])

    def test_container_without_mpi_libraries_major(self):
        self._test_container_without_mpi_libraries(_get_hook_config_and_libs_major()[0])

    def test_container_without_mpi_libraries_full(self):
        self._test_container_without_mpi_libraries(_get_hook_config_and_libs_full()[0])

    def test_container_without_mpi_libraries_strict(self):
        self._test_container_without_mpi_libraries(_get_hook_config_and_libs_strict()[0])

    def _test_container_without_mpi_libraries(self, hook_config):
        check_exception_message(
            hook_config=hook_config,
            container_image="quay.io/ethcscs/sarus-integration-tests:no_mpi_libraries",
            error_regex="No MPI libraries found in the container"
        )

    def test_container_without_mpi_libraries_and_nonexisting_ldcache_entry_default(self):
        self._test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(_get_hook_config_and_libs_default()[0])

    def test_container_without_mpi_libraries_and_nonexisting_ldcache_entry_major(self):
        self._test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(_get_hook_config_and_libs_major()[0])

    def test_container_without_mpi_libraries_and_nonexisting_ldcache_entry_full(self):
        self._test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(_get_hook_config_and_libs_full()[0])

    def test_container_without_mpi_libraries_and_nonexisting_ldcache_entry_strict(self):
        self._test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(_get_hook_config_and_libs_strict()[0])

    def _test_container_without_mpi_libraries_and_nonexisting_ldcache_entry(self, hook_config):
        check_exception_message(
            hook_config=hook_config,
            container_image="quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry_f35",
            error_regex="No MPI libraries found in the container"
        )


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
        hook_config, _ = _get_hook_config_and_libs_default()
        hook_config["hook"]["env"].append("BIND_MOUNTS=/dev/test0:/var/opt:/var/lib")
        return hook_config

    def test_whitelist_device(self):
        with util.temporary_hook_files((self._generate_hook_config_with_device(), _OCIHOOK_CONFIG_FILE)):
            devices_list = self._get_devices_list_from_cgroup_in_container()
            assert "c 511:511 rw" in devices_list

    def _get_devices_list_from_cgroup_in_container(self):
        return util.run_command_in_container(is_centralized_repository=False,
                                             image=self.CONTAINER_IMAGE,
                                             command=["cat", "/sys/fs/cgroup/devices/devices.list"],
                                             options_of_run_command=["--mpi"])
