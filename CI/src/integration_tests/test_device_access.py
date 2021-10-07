# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import pytest

import common.util as util


@pytest.mark.asroot
class TestDeviceAccess(unittest.TestCase):
    """
    These tests verify that within Sarus containers read/write access to device
    files is not permitted by default (denied by cgroup settings).
    """

    DEVICE_FILENAME = "/dev/test0"

    @classmethod
    def setUpClass(cls):
        cls._create_device_file()
        cls.container_image = "ubuntu:20.04"
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image=cls.container_image)
    @classmethod
    def tearDownClass(cls):
        os.remove(cls.DEVICE_FILENAME)

    @classmethod
    def _create_device_file(cls):
        import stat
        device_mode = 0o666 | stat.S_IFCHR
        device_id = os.makedev(511, 511)
        os.mknod(cls.DEVICE_FILENAME, device_mode, device_id)

    def test_device_read_access(self):
        sarus_options = ["--mount=type=bind,source=" + self.__class__.DEVICE_FILENAME
                         + ",destination=" + self.__class__.DEVICE_FILENAME]
        container_args = ["bash", "-c", "exec 3< /dev/test0"]
        self._assert_sarus_raises_error_containing_text(sarus_options, container_args, "Operation not permitted")

    def test_device_write_access(self):
        sarus_options = ["--mount=type=bind,source=" + self.__class__.DEVICE_FILENAME
                         + ",destination=" + self.__class__.DEVICE_FILENAME]
        container_args = ["bash", "-c", "exec 3> /dev/test0"]
        self._assert_sarus_raises_error_containing_text(sarus_options, container_args, "Operation not permitted")

    def _assert_sarus_raises_error_containing_text(self, sarus_options, container_args, text):
        sarus_output = self._get_sarus_error_output(sarus_options, container_args)
        assert text in sarus_output, 'Sarus didn\'t generate an error containing the text "{}", but one was expected.'.format(text)

    def _get_sarus_error_output(self, sarus_options, container_args):
        command = ["sarus", "run"] + sarus_options +[self.__class__.container_image] + container_args
        try:
            subprocess.check_output(command, stderr=subprocess.STDOUT)
            raise Exception(
                "Sarus didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            return ex.output.decode()