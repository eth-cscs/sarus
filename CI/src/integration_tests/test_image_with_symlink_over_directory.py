# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestImageWithSymlinkOverDirectory(unittest.TestCase):
    """
    These tests simply pull and run an image where a directory was
    created in a layer and then replaced with a symlink in a later layer.
    """

    _IMAGE_NAME = "quay.io/ethcscs/sarus-integration-tests:symlink-over-directory"

    def test_image_with_symlink_over_directory(self):
        util.remove_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["ls", "/usr/local/test".encode('utf-8')])
        assert output[0] == "file"
        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["realpath", "/usr/local/test".encode('utf-8')])
        assert output[0] == "/opt/test"
