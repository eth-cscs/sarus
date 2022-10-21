# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestRamFilesystems(unittest.TestCase):
    """
    These tests verify that containers can run using different in-memory filesystems for the OCI bundle.
    """
    _CONTAINER_IMAGE = util.ALPINE_IMAGE

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls._CONTAINER_IMAGE)

    def test_tmpfs(self):
        with util.custom_sarus_json({"ramFilesystemType": "tmpfs"}):
            self._run_test()

    def test_ramfs(self):
        with util.custom_sarus_json({"ramFilesystemType": "ramfs"}):
            self._run_test()

    def _run_test(self):
        prettyname = util.run_image_and_get_prettyname(False, self._CONTAINER_IMAGE)
        assert prettyname.startswith("Alpine Linux")
