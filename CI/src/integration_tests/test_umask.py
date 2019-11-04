# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import os
import unittest

import common.util as util


class TestUmask(unittest.TestCase):
    """
    This test verifies that Sarus is able to run under different umask configurations.
    """

    _IMAGE_NAME = "alpine:latest"
    _umask_backup = None

    @classmethod
    def setUpClass(cls):
        cls._umask_backup = os.umask(0) # set dummy umask to read current value
        os.umask(cls._umask_backup) # reset umask

    @classmethod
    def tearDownClass(cls):
        os.umask(cls._umask_backup)

    def test_umask(self):
        umask_backup = os.umask(0000)
        self._run_sarus()

        os.umask(0777)
        self._run_sarus()

        os.umask(umask_backup)

    def _run_sarus(self):
        util.generate_ssh_keys()
        util.remove_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        util.run_command_in_container(is_centralized_repository=False,
                                      image=self._IMAGE_NAME,
                                      command=["true"])
