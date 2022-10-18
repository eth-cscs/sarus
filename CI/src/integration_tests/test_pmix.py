# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import shutil
import subprocess

import common.util as util


class TestPmixSupport(unittest.TestCase):
    """
    These tests verify that bind mounts related to PMIx v3 are correctly performed by Sarus.
    """

    _IMAGE_NAME = "quay.io/ethcscs/alpine:3.14"

    @classmethod
    def setUpClass(cls):
        cls._modify_sarusjson_file()
        cls._create_pmix_server_tmpdir()
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls._IMAGE_NAME)

    @classmethod
    def tearDownClass(cls):
        cls._restore_sarusjson_file()
        cls._remove_pmix_server_tmpdir()

    @classmethod
    def _modify_sarusjson_file(cls):
        cls._sarusjson_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"

        # Create backup
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename, cls._sarusjson_filename+'.bak'])

        # Modify config file
        import json
        with open(cls._sarusjson_filename, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["enablePMIxv3Support"] = True
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", cls._sarusjson_filename])
        os.remove("sarus.json.dummy")

    @classmethod
    def _restore_sarusjson_file(cls):
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename+'.bak', cls._sarusjson_filename])

    @classmethod
    def _create_pmix_server_tmpdir(cls):
        cls.pmix_server_tmpdir = os.path.join(os.getcwd(), "sarus-test", "pmix_server_tmpdir")
        os.makedirs(cls.pmix_server_tmpdir, exist_ok=True)

    @classmethod
    def _remove_pmix_server_tmpdir(cls):
        shutil.rmtree(cls.pmix_server_tmpdir)

    def test_server_tmpdir_mount(self):
        host_environment = os.environ.copy()
        host_environment["PMIX_SERVER_TMPDIR"] = self.__class__.pmix_server_tmpdir

        expected_file = os.path.join(self.__class__.pmix_server_tmpdir, "test-file")
        expected_string = "PMIx-test-value"
        with open(expected_file, 'w') as f:
            f.write(expected_string)

        command = ["sh", "-c", f"cat {expected_file}"]
        try:
            output = util.run_command_in_container(is_centralized_repository=False,
                                                   image=self._IMAGE_NAME,
                                                   command=command,
                                                   env=host_environment)
            self.assertTrue(output == [expected_string])
        finally:
            os.remove(expected_file)
