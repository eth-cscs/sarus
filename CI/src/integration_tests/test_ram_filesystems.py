# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import json

import common.util as util


class TestRamFilesystem(unittest.TestCase):
    """
    Base class for rootfs in-memory filesystems testing.
    """
    _SARUS_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"
    _CONTAINER_IMAGE = "alpine:3.8"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls._CONTAINER_IMAGE)
        cls._enable_hook()

    @classmethod
    def tearDownClass(cls):
        cls._disable_hook()

    @classmethod
    def _enable_hook(cls):
        # make backup of config file
        subprocess.call(["sudo", "cp", cls._SARUS_CONFIG_FILE, cls._SARUS_CONFIG_FILE + ".bak"])

        # Modify config file
        with open(cls._SARUS_CONFIG_FILE, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["ramFilesystemType"] = cls._FILESYSTEM_TYPE
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", cls._SARUS_CONFIG_FILE])
        os.remove("sarus.json.dummy")

    @classmethod
    def _disable_hook(cls):
        subprocess.call(["sudo", "mv", cls._SARUS_CONFIG_FILE + ".bak", cls._SARUS_CONFIG_FILE])

class TestRamfs(TestRamFilesystem):
    """
    These tests verify that containers can run using ramfs as filesystem for the OCI bundle.
    """
    _FILESYSTEM_TYPE = "ramfs"

    def test_ramfs(self):
        self.assertEqual(util.run_image_and_get_prettyname(False, "alpine:3.8"),
            "Alpine Linux")

class TestTmpfs(TestRamFilesystem):
    """
    These tests verify that containers can run using tmpfs as filesystem for the OCI bundle.
    """
    _FILESYSTEM_TYPE = "tmpfs"

    def test_tmpfs(self):
        self.assertEqual(util.run_image_and_get_prettyname(False, "alpine:3.8"),
            "Alpine Linux")