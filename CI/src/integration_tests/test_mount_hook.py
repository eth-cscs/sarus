# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import shutil
import pytest

import common.util as util


class TestMountHook(unittest.TestCase):
    """
    These tests verify that the features of the Mount hook (bind mounts, device mounts, wildcards resolution)
    work correctly.
    """
    OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/mount_hook.json"
    CONTAINER_IMAGE = "quay.io/ethcscs/ubuntu:20.04"

    _CI_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    _DUMMY_LIB_PATH = _CI_DIR + "/dummy_libs/lib_dummy_0.so"
    HOST_LIB_HASH = util.generate_file_md5_hash(_DUMMY_LIB_PATH, "md5")

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image=cls.CONTAINER_IMAGE)
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image="quay.io/ethcscs/sarus-integration-tests:libfabric")

    @staticmethod
    def _generate_hook_config(args, with_ldconfig=False):
        hook_config = {
            "version": "1.0.0",
            "hook": {
                "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/mount_hook",
                "args": ["mount_hook"] + args
            },
            "when": {
                "always": True
            },
            "stages": ["prestart"]
        }
        if with_ldconfig:
            hook_config["hook"]["env"] = ["LDCONFIG_PATH=" + shutil.which("ldconfig")]
        return hook_config

    def test_bind_mount(self):
        mount_destination = "/usr/lib64/libMountHook.so.1"
        self._check_hook_mount(hook_config=self._generate_hook_config_with_dummylib_mount(mount_destination),
                               expected_destination=mount_destination)

    def test_ldcache_update(self):
        mount_destination = "/usr/local/lib/libMountHook.so.1"
        hook_config = self._generate_hook_config_with_dummylib_mount(mount_destination, with_ldconfig=True)
        with util.temporary_hook_files((hook_config, self.OCIHOOK_CONFIG_FILE)):
            ldcache_list = util.run_command_in_container(is_centralized_repository=False,
                                                         image=self.CONTAINER_IMAGE,
                                                         command=["/sbin/ldconfig", "-p"])
            assert any(mount_destination in line for line in ldcache_list)

    def test_fi_provider_path_wildcard_default(self):
        self._check_hook_mount(hook_config=self._generate_hook_config_with_wildcard_mount(),
                               expected_destination="/usr/lib/provider-fi.so")

    def test_fi_provider_path_wildcard_from_environment(self):
        env_var_value = "/fi/provider/path"
        self._check_hook_mount(hook_config=self._generate_hook_config_with_wildcard_mount(),
                               expected_destination=f"{env_var_value}/provider-fi.so",
                               options=[f"--env=FI_PROVIDER_PATH={env_var_value}"])

    def test_fi_provider_path_wildcard_from_environment_precedes_ldcache(self):
        env_var_value = "/fi/provider/path"
        self._check_hook_mount(hook_config=self._generate_hook_config_with_wildcard_mount(with_ldconfig=True),
                               expected_destination=f"{env_var_value}/provider-fi.so",
                               image="quay.io/ethcscs/sarus-integration-tests:libfabric",
                               options=[f"--env=FI_PROVIDER_PATH={env_var_value}"])

    def test_fi_provider_path_wildcard_from_ldcache(self):
        self._check_hook_mount(hook_config=self._generate_hook_config_with_wildcard_mount(with_ldconfig=True),
                               expected_destination="/usr/lib/libfabric/provider-fi.so",
                               image="quay.io/ethcscs/sarus-integration-tests:libfabric")

    def _generate_hook_config_with_dummylib_mount(self, mount_destination, **kwargs):
        return self._generate_hook_config([f"--mount=type=bind,src={self._DUMMY_LIB_PATH},dst={mount_destination}"],
                                          **kwargs)

    def _generate_hook_config_with_wildcard_mount(self, **kwargs):
        return self._generate_hook_config_with_dummylib_mount("<FI_PROVIDER_PATH>/provider-fi.so", **kwargs)

    def _check_hook_mount(self, hook_config, expected_destination, image=CONTAINER_IMAGE, **kwargs):
        with util.temporary_hook_files((hook_config, self.OCIHOOK_CONFIG_FILE)):
            file_hash = util.get_hash_of_file_in_container(expected_destination, image, **kwargs)
            assert file_hash == self.HOST_LIB_HASH


@pytest.mark.asroot
class TestMountHookDevices(unittest.TestCase):
    """
    These tests verify that the mount hook is able to mount and whitelist devices
    for access in the container devices cgroup.
    """
    OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/mount_hook.json"
    DEVICE_FILENAME = "/dev/test0"
    CONTAINER_IMAGE = "quay.io/ethcscs/ubuntu:20.04"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        cls._create_device_file()

    @classmethod
    def tearDownClass(cls):
        os.remove(cls.DEVICE_FILENAME)

    @classmethod
    def _create_device_file(cls):
        import stat
        device_mode = 0o666 | stat.S_IFCHR
        device_id = os.makedev(511, 511)
        os.mknod(cls.DEVICE_FILENAME, device_mode, device_id)

    def test_whitelist_device(self):
        hook_config = TestMountHook._generate_hook_config([f"--device={self.DEVICE_FILENAME}:rw"])
        with util.temporary_hook_files((hook_config, self.OCIHOOK_CONFIG_FILE)):
            devices_list = self._get_devices_list_from_cgroup_in_container()
            assert "c 511:511 rw" in devices_list

    def _get_devices_list_from_cgroup_in_container(self):
        return util.run_command_in_container(is_centralized_repository=False,
                                             image=self.CONTAINER_IMAGE,
                                             command=["cat", "/sys/fs/cgroup/devices/devices.list"])
