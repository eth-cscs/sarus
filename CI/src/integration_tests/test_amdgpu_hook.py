# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import collections
import os
import subprocess
import unittest

import common.util as util
import pytest
from integration_tests.test_device_access import Device

DeviceLink = collections.namedtuple('Device', ['source', 'destination'])


def create_paths_for_devices(devices, device_links):
    # devices
    paths = {os.path.dirname(dev.path)
             for dev in devices if os.path.dirname(dev.path) != '/dev'}
    for p in paths:
        os.makedirs(p, exist_ok=True)
    # links
    paths = {
        os.path.dirname(dev.destination) for dev in device_links if
        os.path.dirname(dev.destination) != '/dev'
    }
    for p in paths:
        os.makedirs(p, exist_ok=True)


def remove_paths_for_devices(devices, device_links):

    def remove_paths(paths):
        for p in sorted(paths, reverse=True):
            try:
                os.rmdir(p)
            except OSError as e:
                print(e)

    paths = {os.path.dirname(dev.destination) for dev in device_links}
    remove_paths(paths)

    paths = {os.path.dirname(dev.path)
             for dev in devices if os.path.dirname(dev.path) != '/dev'}
    remove_paths(paths)


def create_device_files(devices, device_links):
    import stat
    device_mode = 0o666 | stat.S_IFCHR

    # create devices
    for device in devices:
        try:
            device_id = os.makedev(device.id, device.id)
            os.mknod(device.path, device_mode, device_id)
        except Exception as e:
            print(f"{device.path} : {e}")

    workdir = os.getcwd()
    # create links
    for (source, destination) in device_links:
        os.chdir(os.path.dirname(destination))
        try:
            os.chdir(os.path.dirname(destination))
            os.symlink(src=f'../{os.path.basename(source)}',
                       dst=os.path.basename(destination))
        except Exception as e:
            print(f"{source} : {e}")
    os.chdir(workdir)


def remove_device_files(devices, device_links):

    def remove_device_files_impl(filename):
        try:
            os.remove(filename)
        except Exception as e:
            print(f"{filename}: {e}")

    # remove links
    for (_, destination) in device_links:
        remove_device_files_impl(destination)

    # remove devices
    for device in devices:
        remove_device_files_impl(device.path)


def create_hook_file(oci_hook_config_file):
    hook = {
        "version": "1.0.0",
        "hook": {
            "path": "/opt/sarus/default/bin/amdgpu_hook"
        },
        "when": {
            "always": True
        },
        "stages": ["prestart"]
    }
    util.create_hook_file(hook, oci_hook_config_file)


def remove_hook_file(oci_hook_config_file):
    subprocess.call(["sudo", "rm", oci_hook_config_file])


@pytest.mark.asroot
class TestAmdGpuHook(unittest.TestCase):
    CONTAINER_IMAGE = "alpine:3.15"

    DEVICES = list((Device(name, 511) for name in [
        '/dev/kfd',
        '/dev/dri/card0',
        '/dev/dri/renderD128',
        '/dev/dri/card1',
        '/dev/dri/renderD129',
        '/dev/dri/card2',
        '/dev/dri/renderD130',
        '/dev/dri/card3',
        '/dev/dri/renderD131',
    ]))

    DEVICE_LINKS = list((DeviceLink(source, destination))
                        for source, destination in [
        ('/dev/dri/card0', '/dev/dri/by-path/pci-00:00.0-card'),
        ('/dev/dri/card1', '/dev/dri/by-path/pci-04:00.0-card'),
        ('/dev/dri/card2', '/dev/dri/by-path/pci-08:00.0-card'),
        ('/dev/dri/card3', '/dev/dri/by-path/pci-0C:00.0-card'),
        ('/dev/dri/renderD128', '/dev/dri/by-path/pci-00:00.0-render'),
        ('/dev/dri/renderD129', '/dev/dri/by-path/pci-04:00.0-render'),
        ('/dev/dri/renderD130', '/dev/dri/by-path/pci-08:00.0-render'),
        ('/dev/dri/renderD131', '/dev/dri/by-path/pci-0C:00.0-render'),
    ])

    OCIHOOK_CONFIG_FILE = "/opt/sarus/default/etc/hooks.d/amdgpu_hook.json"

    @classmethod
    def setUpClass(cls):
        create_paths_for_devices(cls.DEVICES, cls.DEVICE_LINKS)
        create_hook_file(cls.OCIHOOK_CONFIG_FILE)
        try:
            util.pull_image_if_necessary(
                is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        except Exception as e:
            print(e)

    @classmethod
    def tearDownClass(cls):
        remove_device_files(cls.DEVICES, cls.DEVICE_LINKS)
        remove_paths_for_devices(cls.DEVICES, cls.DEVICE_LINKS)
        remove_hook_file(cls.OCIHOOK_CONFIG_FILE)

    # @classmethod
    def setUp(self):
        create_device_files(self.DEVICES, self.DEVICE_LINKS)

    def test_absence_of_dev_kfd_disables_hook(self):
        remove_device_files([Device("/dev/kfd", 511)], [])

        output = util.run_command_in_container(
            is_centralized_repository=False,
            image=self.CONTAINER_IMAGE,
            command=["ls", "/dev"]
        )
        assert "dri" not in output

    def test_mount_subset_of_devices(self):
        def test_mount_subset_of_devices_impl(visible_devices):

            host_environment = os.environ.copy()
            host_environment["ROCR_VISIBLE_DEVICES"] = ",".join(
                f"{dev}" for dev in visible_devices)
            try:
                create_hook_file(self.OCIHOOK_CONFIG_FILE)
                output = util.run_command_in_container(
                    is_centralized_repository=False,
                    image=self.CONTAINER_IMAGE,
                    command=["ls", "/dev"],
                    env=host_environment
                )
            except Exception as e:
                assert False, e
            assert all(dev in output for dev in ['kfd', 'dri'])

            try:
                output = util.run_command_in_container(
                    is_centralized_repository=False,
                    image=self.CONTAINER_IMAGE,
                    command=["ls", "/dev/dri"],
                    env=host_environment
                )
            except Exception as e:
                assert False, e
            expects = {f"card{device_id}" for device_id in visible_devices}
            expects |= {
                f"renderD{128+device_id}" for device_id in visible_devices}

            assert sorted(expects) == sorted(output)

        test_mount_subset_of_devices_impl([0, 1, 2])
        test_mount_subset_of_devices_impl([0, 1, 3])
        test_mount_subset_of_devices_impl([0, 2, 3])
        test_mount_subset_of_devices_impl([1, 2, 3])

    def test_mount_all_devices(self):
        try:
            create_hook_file(self.OCIHOOK_CONFIG_FILE)
            output = util.run_command_in_container(
                is_centralized_repository=False,
                image=self.CONTAINER_IMAGE,
                command=["ls", "/dev"]
            )
        except Exception as e:
            assert False, e
        assert all(dev in output for dev in ['kfd', 'dri'])

        try:
            output = util.run_command_in_container(
                is_centralized_repository=False,
                image=self.CONTAINER_IMAGE,
                command=["ls", "/dev/dri"],
            )
        except Exception as e:
            assert False, e

        expects = {f"card{device_id}" for device_id in range(0, 4)}
        expects |= {f"renderD{128+device_id}" for device_id in range(0, 4)}

        assert sorted(expects) == sorted(output)

    def test_whitelist_devices(self):
        devices_list = util.run_command_in_container(
            is_centralized_repository=False,
            image=self.CONTAINER_IMAGE,
            command=["cat", "/sys/fs/cgroup/devices/devices.list"]
        )
        assert "c 511:511 rw" in devices_list

    def test_mounted_devices_have_same_stat_as_original(self):
        host_devices = {dev.path: dev.stat()
                        for dev in os.scandir('/dev/dri') if not dev.is_dir()}
        container_devices = {dev: util.run_command_in_container(
            is_centralized_repository=False, image=self.CONTAINER_IMAGE,
            command=["stat", "-t", dev])[0].split() for dev in host_devices.keys()
        }

        for device, host_stat in host_devices.items():
            container_stat = container_devices.get(device)
            assert int(container_stat[3], 16) == host_stat.st_mode
            assert int(container_stat[7]) == host_stat.st_ino
            assert int(container_stat[11]) == pytest.approx(
                host_stat.st_atime, abs=1)
            assert int(container_stat[12]) == pytest.approx(
                host_stat.st_mtime, abs=1)
            assert int(container_stat[13]) == pytest.approx(
                host_stat.st_ctime, abs=1)
