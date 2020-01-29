# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestContainerIsWritable(unittest.TestCase):
    """
    This test checks that the container's rootfs is writable.
    """

    _IMAGE_NAME = "alpine"

    def test_image_with_max_path_length(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        util.run_command_in_container(is_centralized_repository=False, image=self._IMAGE_NAME, command=["touch", "/file"])
        util.run_command_in_container(is_centralized_repository=False, image=self._IMAGE_NAME, command=["touch", "/etc/file"])
        util.run_command_in_container(is_centralized_repository=False, image=self._IMAGE_NAME, command=["touch", "/bin/file"])
        util.run_command_in_container(is_centralized_repository=False, image=self._IMAGE_NAME, command=["touch", "/sbin/file"])
