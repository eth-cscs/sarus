# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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
    DEFAULT_IMAGE = util.ALPINE_IMAGE

    def test_command_pull_with_local_repository(self):
        self._test_command_pull(self.DEFAULT_IMAGE, is_centralized_repository=False)

    def test_command_pull_with_centralized_repository(self):
        self._test_command_pull(self.DEFAULT_IMAGE, is_centralized_repository=True)

    def test_command_pull_with_nvcr(self):
        self._test_command_pull("nvcr.io/nvidia/k8s-device-plugin:1.0.0-beta4",
                                is_centralized_repository=False)

    def test_command_pull_with_dockerhub(self):
        self._test_command_pull("alpine:latest",
                                is_centralized_repository=False)

    def test_command_pull_with_buildah_image(self):
        self._test_command_pull("quay.io/ethcscs/alpine:buildah",
                                is_centralized_repository=False)

    def test_command_pull_by_digest(self):
        self._test_command_pull("quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                is_centralized_repository=False,
                                expected_string="quay.io/ethcscs/alpine:<none>@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

    def test_command_pull_by_tag_and_digest(self):
        self._test_command_pull("quay.io/ethcscs/alpine:3.14@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                is_centralized_repository=False,
                                expected_string="quay.io/ethcscs/alpine:<none>@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

    def test_command_pull_with_uptodate_image_in_local_repository(self):
        self._test_command_pull_with_uptodate_image(self.DEFAULT_IMAGE, False)

    def test_command_pull_with_uptodate_image_in_centralized_repository(self):
        self._test_command_pull_with_uptodate_image(self.DEFAULT_IMAGE, True)

    def test_command_pull_with_uptodate_image_by_digest(self):
        self._test_command_pull_with_uptodate_image("quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                                    False)

    def test_command_pull_with_uptodate_image_by_tag_and_digest(self):
        self._test_command_pull_with_uptodate_image("quay.io/ethcscs/alpine:3.14@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                                    False)

    def test_multiple_pulls_with_local_repository(self):
        self._test_multiple_pulls(self.DEFAULT_IMAGE, is_centralized_repository=False)

    def test_multiple_pulls_with_centralized_repository(self):
        self._test_multiple_pulls(self.DEFAULT_IMAGE, is_centralized_repository=True)

    def test_pull_with_custom_mksquashfs_options(self):
        with util.custom_sarus_json({"mksquashfsOptions": "-comp lz4 -processors 2"}):
            image = self.DEFAULT_IMAGE
            util.remove_image_if_necessary(is_centralized_repository=False, image=image)

            # Check that image can be pulled and run
            util.pull_image(is_centralized_repository=False, image=image)
            assert util.run_image_and_get_prettyname(is_centralized_repository=False,
                                                     image=image).startswith("Alpine Linux")
            util.remove_image_if_necessary(is_centralized_repository=False, image=image)

    def _test_command_pull(self, image, is_centralized_repository, expected_string=None):
        expected_image = expected_string if expected_string else image

        util.remove_image_if_necessary(is_centralized_repository, image)
        self.assertFalse(util.is_image_available(is_centralized_repository, expected_image))

        util.pull_image_if_necessary(is_centralized_repository, image)
        self.assertTrue(util.is_image_available(is_centralized_repository, expected_image))

    def _test_command_pull_with_uptodate_image(self, image, is_centralized_repository):
        util.pull_image_if_necessary(is_centralized_repository, image)
        output = util.pull_image(is_centralized_repository, image)
        self.assertEqual(output[-1], f"Image for {image} is already available and up to date")

    # check that multiple pulls of the same image don't generate
    # multiple entries in the list of available images
    def _test_multiple_pulls(self, image, is_centralized_repository):
        for i in range(2):
            util.pull_image(is_centralized_repository, image)
            actual_images = util.list_images(is_centralized_repository)
            self.assertEqual(actual_images.count(image), 1)
