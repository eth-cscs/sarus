# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import pytest
import collections

import common.util as util

Device = collections.namedtuple('Device', ['path', 'id'])


class TestDeviceBaseClass(unittest.TestCase):
    DEVICES = []

    @classmethod
    def setUpClass(cls):
        cls._create_device_files()
        cls.container_image = "quay.io/ethcscs/ubuntu:20.04"
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image=cls.container_image)
    @classmethod
    def tearDownClass(cls):
        cls._remove_device_files()

    @classmethod
    def _create_device_files(cls):
        import stat
        device_mode = 0o666 | stat.S_IFCHR
        for device in cls.DEVICES:
            device_id = os.makedev(device.id, device.id)
            os.mknod(device.path, device_mode, device_id)

    @classmethod
    def _remove_device_files(cls):
        for device in cls.DEVICES:
            os.remove(device.path)

    def _get_devices_list_from_cgroup_in_container(self, sarus_options):
        return util.run_command_in_container(is_centralized_repository=False,
                                             image=self.__class__.container_image,
                                             command=["cat",
                                                      "/sys/fs/cgroup/devices/devices.list"],
                                             options_of_run_command=sarus_options)

    def _device_files_exist_in_container(self, file_paths, sarus_options):
        file_names = ['"{}"'.format(fpath) for fpath in file_paths]
        check_script = (f"for fname in {' '.join(file_names)}; do"
                         "    if [ ! -c $fname ]; then"
                         "         echo \"FAIL\"; "
                         "    fi; "
                         "done; "
                         "echo \"PASS\" ")
        command = ["bash", "-c"] + [check_script]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.__class__.container_image,
                                            command=command,
                                            options_of_run_command=sarus_options)
        return out == ["PASS"]

    def _assert_sarus_raises_error_containing_text(self, sarus_options, container_args, text):
        sarus_output = self._get_sarus_error_output(sarus_options, container_args)
        assert text in sarus_output, 'Sarus generated an error, but it did not contain the expected text "{}".'.format(text)

    def _get_sarus_error_output(self, sarus_options, container_args):
        command = ["sarus", "run"] + sarus_options +[self.__class__.container_image] + container_args
        try:
            subprocess.check_output(command, stderr=subprocess.STDOUT)
            raise Exception(
                "Sarus didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            return ex.output.decode()


@pytest.mark.asroot
class TestDeviceDefaultAccess(TestDeviceBaseClass):
    """
    These tests verify that within Sarus containers read/write access to device
    files is not permitted by default (denied by cgroup settings).
    """
    DEVICES = [Device("/dev/sarus-test0",511)]

    def test_mounted_device_read_access(self):
        device_path = self.__class__.DEVICES[0].path
        sarus_options = ["--mount=type=bind,source=" + device_path
                         + ",destination=" + device_path]
        container_args = ["bash", "-c", "exec 3< /dev/sarus-test0"]
        self._assert_sarus_raises_error_containing_text(sarus_options, container_args, "Operation not permitted")

    def test_mounted_device_write_access(self):
        device_path = self.__class__.DEVICES[0].path
        sarus_options = ["--mount=type=bind,source=" + device_path
                         + ",destination=" + device_path]
        container_args = ["bash", "-c", "exec 3> /dev/sarus-test0"]
        self._assert_sarus_raises_error_containing_text(sarus_options, container_args, "Operation not permitted")


@pytest.mark.asroot
class TestDeviceOption(TestDeviceBaseClass):
    """
    These tests verify that the --device option of sarus run result in mounting
    a device file within the container, and that such file is whitelisted in the
    container's devices cgroup.
    A test also verifies that Sarus cannot grant more permissions than the host
    cgroup
    For additional reference to the interface of the devices cgroup and how to
    allow/deny devices see https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v1/devices.html
    """
    DEVICES = [Device("/dev/sarus-test0",505)]

    def test_device_mount(self):
        # only source path
        device_path = self.__class__.DEVICES[0].path
        sarus_options = ["--device=" + device_path]
        self.assertTrue(self._device_files_exist_in_container([device_path], sarus_options))

        # source + destination
        destination_path = "/dev/newDevice0"
        option_value = device_path + ":" + destination_path
        sarus_options = ["--device=" + option_value]
        self.assertTrue(self._device_files_exist_in_container([destination_path], sarus_options))

    def test_whitelist_device(self):
        device_path = self.__class__.DEVICES[0].path
        device_id = self.__class__.DEVICES[0].id

        # only source path
        sarus_options = ["--device=" + device_path]
        devices_list = self._get_devices_list_from_cgroup_in_container(sarus_options)
        expected_entry = f"c {device_id}:{device_id} rwm"
        assert expected_entry in devices_list

        # source path + access
        sarus_options = ["--device=" + device_path + ":r"]
        devices_list = self._get_devices_list_from_cgroup_in_container(sarus_options)
        expected_entry = f"c {device_id}:{device_id} r"
        assert expected_entry in devices_list

        # source + destination + access
        destination_path = "/dev/newDevice0"
        option_value = device_path + ":" + destination_path + ":rw"
        sarus_options = ["--device=" + option_value]
        devices_list = self._get_devices_list_from_cgroup_in_container(sarus_options)
        expected_entry = f"c {device_id}:{device_id} rw"
        assert expected_entry in devices_list

    def test_no_additional_permissions(self):
        # Restrict device access in host cgroup to read-only
        device_id = self.__class__.DEVICES[0].id
        subprocess.check_output(["bash", "-c", f"echo 'c {device_id}:{device_id} w' > /sys/fs/cgroup/devices/devices.deny"])

        # Check device mount with default 'rwm' access fails
        device_path = self.__class__.DEVICES[0].path
        sarus_options = ["--device=" + device_path]
        container_args = ["ls", device_path]
        self._assert_sarus_raises_error_containing_text(sarus_options, container_args, "operation not permitted")

        # Check device mount with restricted read-only access works
        sarus_options = ["--device=" + device_path + ":r"]
        container_args = ["ls", device_path]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.__class__.container_image,
                                            command=container_args,
                                            options_of_run_command=sarus_options)
        assert out[0] == device_path

        # Restore full device access in host cgroup
        subprocess.check_output(["bash", "-c", f"echo 'c {device_id}:{device_id} w' > /sys/fs/cgroup/devices/devices.allow"])

        # Check device mount with default access works again
        sarus_options = ["--device=" + device_path]
        container_args = ["ls", device_path]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.__class__.container_image,
                                            command=container_args,
                                            options_of_run_command=sarus_options)
        assert out[0] == device_path


@pytest.mark.asroot
class TestSiteDevices(TestDeviceBaseClass):
    """
    These tests verify the functionalities of the siteDevices parameter of sarus.json:
        - creating entries results in mounting the corresponding file in the
        container
        - such file is whitelisted in the devices cgroup
        - the --device CLI option can co-operate with the config file setting
    """
    DEVICES = [Device("/dev/sarus-test0",500), Device("/dev/sarus-test1",501),
               Device("/dev/sarus-test2",502), Device("/dev/sarus-test3", 503),
               Device("/dev/sarus-test4", 504)]

    @classmethod
    def setUpClass(cls):
        TestDeviceBaseClass.setUpClass()
        cls._create_device_files()
        cls._modify_sarusjson_file()

    @classmethod
    def tearDownClass(cls):
        cls._remove_device_files()
        cls._restore_sarusjson_file()

    @classmethod
    def _modify_sarusjson_file(cls):
        import json
        cls._sarusjson_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"

        # Create backup
        subprocess.check_output(["cp", cls._sarusjson_filename, cls._sarusjson_filename+'.bak'])

        # Build site devices
        site_devices = [{"source": cls.DEVICES[0].path},
                        {"source": cls.DEVICES[1].path, "destination": "/dev/site1"},
                        {"source": cls.DEVICES[2].path, "access": "rw"},
                        {"source": cls.DEVICES[3].path, "destination": "/dev/site2", "access": "r"}]

        # Modify config file
        with open(cls._sarusjson_filename, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["siteDevices"] = site_devices
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["cp", "sarus.json.dummy", cls._sarusjson_filename])
        os.remove("sarus.json.dummy")

    @classmethod
    def _restore_sarusjson_file(cls):
        subprocess.check_output(["cp", cls._sarusjson_filename+'.bak', cls._sarusjson_filename])

    def test_site_device_mount(self):
        expected_paths = [self.__class__.DEVICES[0].path,
                        "/dev/site1",
                        self.__class__.DEVICES[2].path,
                        "/dev/site2"]
        self.assertTrue(self._device_files_exist_in_container(expected_paths, sarus_options=[]))

    def test_site_device_and_option_mount(self):
        expected_paths = [self.__class__.DEVICES[0].path,
                          "/dev/site1",
                          self.__class__.DEVICES[2].path,
                          "/dev/site2",
                          self.__class__.DEVICES[4].path]
        sarus_options = ["--device=" + expected_paths[4]]
        self.assertTrue(self._device_files_exist_in_container(expected_paths, sarus_options))

    def test_whitelist_devices(self):
        device_ids = [device.id for device in self.__class__.DEVICES]
        expected_entries = [f"c {device_ids[0]}:{device_ids[0]} rwm",
                            f"c {device_ids[1]}:{device_ids[1]} rwm",
                            f"c {device_ids[2]}:{device_ids[2]} rw",
                            f"c {device_ids[3]}:{device_ids[3]} r"]
        devices_list = self._get_devices_list_from_cgroup_in_container(sarus_options=[])
        assert all((entry in devices_list for entry in expected_entries))

        # CLI option cannot override permissions from siteDevices entry
        sarus_options = ["--device=" + self.__class__.DEVICES[3].path + ":rwm"]
        devices_list = self._get_devices_list_from_cgroup_in_container(sarus_options)
        assert all((entry in devices_list for entry in expected_entries))

        # CLI option can add other devices
        sarus_options = ["--device=" + self.__class__.DEVICES[4].path + ":rw"]
        expected_entries.append(f"c {device_ids[4]}:{device_ids[4]} rw")
        devices_list = self._get_devices_list_from_cgroup_in_container(sarus_options)
        assert all((entry in devices_list for entry in expected_entries))
