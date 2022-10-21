# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import shutil

import common.util as util


class TestPmixSupport(unittest.TestCase):
    """
    These tests verify that bind mounts related to PMIx v3 are correctly performed by Sarus.
    """

    CONTAINER_IMAGE = util.ALPINE_IMAGE
    PMIX_SERVER_TMPDIR = None

    @classmethod
    def setUpClass(cls):
        util.modify_sarus_json({"enablePMIxv3Support": True})
        cls._create_pmix_server_tmpdir()
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)

    @classmethod
    def tearDownClass(cls):
        util.restore_sarus_json()
        cls._remove_pmix_server_tmpdir()

    @classmethod
    def _create_pmix_server_tmpdir(cls):
        cls.PMIX_SERVER_TMPDIR = os.path.join(os.getcwd(), "sarus-test", "pmix_server_tmpdir")
        os.makedirs(cls.PMIX_SERVER_TMPDIR, exist_ok=True)

    @classmethod
    def _remove_pmix_server_tmpdir(cls):
        shutil.rmtree(cls.PMIX_SERVER_TMPDIR)

    def test_server_tmpdir_mount(self):
        host_environment = os.environ.copy()
        host_environment["PMIX_SERVER_TMPDIR"] = self.PMIX_SERVER_TMPDIR

        expected_file = os.path.join(self.PMIX_SERVER_TMPDIR, "test-file")
        expected_string = "PMIx-test-value"
        with open(expected_file, 'w') as f:
            f.write(expected_string)

        command = ["sh", "-c", f"cat {expected_file}"]
        try:
            output = util.run_command_in_container(is_centralized_repository=False,
                                                   image=self.CONTAINER_IMAGE,
                                                   command=command,
                                                   env=host_environment)
            self.assertTrue(output == [expected_string])
        finally:
            os.remove(expected_file)
