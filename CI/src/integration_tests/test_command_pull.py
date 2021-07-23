# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestCommandPull(unittest.TestCase):
    """
    These tests verify that the pulled images are available to the user,
    i.e. correctly listed through the "images" command.
    """

    def test_command_pull_with_local_repository(self):
        self._test_command_pull("quay.io/ethcscs/alpine:3.14", is_centralized_repository=False)

    def test_command_pull_with_centralized_repository(self):
        self._test_command_pull("quay.io/ethcscs/alpine:3.14", is_centralized_repository=True)

    def test_command_pull_with_nvcr(self):
        self._test_command_pull("nvcr.io/nvidia/k8s-device-plugin:1.0.0-beta4",
                                is_centralized_repository=False)

    def test_command_pull_with_dockerhub(self):
        self._test_command_pull("ubuntu:20.04",
                                is_centralized_repository=False)

    def test_multiple_pulls_with_local_repository(self):
        self._test_multiple_pulls("quay.io/ethcscs/alpine:3.14", is_centralized_repository=False)

    def test_multiple_pulls_with_centralized_repository(self):
        self._test_multiple_pulls("quay.io/ethcscs/alpine:3.14", is_centralized_repository=True)

    def _test_command_pull(self, image, is_centralized_repository):
        util.remove_image_if_necessary(is_centralized_repository, image)
        actual_images = util.list_images(is_centralized_repository)
        self.assertEqual(actual_images.count(image), 0)

        util.pull_image_if_necessary(is_centralized_repository, image)
        actual_images = util.list_images(is_centralized_repository)
        self.assertEqual(actual_images.count(image), 1)

    # check that multiple pulls of the same image don't generate
    # multiple entries in the list of available images
    def _test_multiple_pulls(self, image, is_centralized_repository):
        for i in range(2):
            util.pull_image(is_centralized_repository, image)
            actual_images = util.list_images(is_centralized_repository)
            self.assertEqual(actual_images.count(image), 1)
