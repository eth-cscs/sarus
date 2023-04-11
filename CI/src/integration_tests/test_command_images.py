# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import re


import common.util as util


class TestCommandImages(unittest.TestCase):
    """
    This test checks the output of the "images" command in different scenarios:
    - when the metadata file doesn't exist.
    - when the metadata file contains images.
    - when the metadata file is empty.
    """

    _IMAGE_NAME = "loaded_image:latest"

    def test_command_images_with_local_repository(self):
        self._test_command_images(is_centralized_repository=False)

    def test_command_images_with_centralized_repository(self):
        self._test_command_images(is_centralized_repository=True)

    def test_command_images_digests(self):
        # Pull by digest: expect only digest and no tag
        self._test_command_images_digests("quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                          expected_name="quay.io/ethcscs/alpine",
                                          expected_tag="<none>",
                                          expected_digest="sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

        # Pull by tag and digest: expect only digest and no tag
        self._test_command_images_digests("quay.io/ethcscs/ubuntu:20.04@sha256:778fdd9f62a6d7c0e53a97489ab3db17738bc5c1acf09a18738a2a674025eae6",
                                          expected_name="quay.io/ethcscs/ubuntu",
                                          expected_tag="<none>",
                                          expected_digest="sha256:778fdd9f62a6d7c0e53a97489ab3db17738bc5c1acf09a18738a2a674025eae6")

        # Pull by tag: expect both tag and digest
        self._test_command_images_digests("quay.io/ethcscs/centos:7",
                                          expected_name="quay.io/ethcscs/centos",
                                          expected_tag="7",
                                          expected_digest="sha256:e4ca2ed0202e76be184e75fb26d14bf974193579039d5573fb2348664deef76e")

    def test_cleanup_image_without_backing_file(self):
        image = util.ALPINE_IMAGE
        util.pull_image_if_necessary(is_centralized_repository=False, image=image)
        util.remove_image_backing_file(image)
        assert not util.is_image_available(is_centralized_repository=False, target_image=image)

    def _test_command_images_digests(self, image, expected_name, expected_tag, expected_digest):
        expected_header = ["REPOSITORY", "TAG", "DIGEST", "IMAGE", "ID", "CREATED", "SIZE", "SERVER"]
        actual_header = self._header_in_output_of_images_command(is_centralized_repository=False, print_digests=True)
        self.assertEqual(actual_header, expected_header)

        util.pull_image_if_necessary(is_centralized_repository=False, image=image)

        image_output = self._image_in_output_of_images_command(is_centralized_repository=False, print_digests=True,
                                                               image=expected_name, tag=expected_tag, digest=expected_digest)
        self.assertEqual(image_output[0], expected_name)
        self.assertEqual(image_output[1], expected_tag)
        self.assertEqual(image_output[2], expected_digest)

    def _test_command_images(self, is_centralized_repository):
        expected_header = ["REPOSITORY", "TAG", "IMAGE", "ID", "CREATED", "SIZE", "SERVER"]

        # pull image from default registry
        util.pull_image_if_necessary(is_centralized_repository, "alpine")

        # load image (with fixed digest)
        image_archive = os.path.dirname(os.path.abspath(__file__)) + "/saved_image.tar"
        util.load_image(is_centralized_repository, image_archive, self._IMAGE_NAME)

        # header
        actual_header = self._header_in_output_of_images_command(is_centralized_repository)
        self.assertEqual(actual_header, expected_header)

        # pulled image from default registry
        image_output = self._image_in_output_of_images_command(is_centralized_repository, False, "alpine", "latest", "")
        self.assertEqual(image_output[0], "alpine")
        self.assertEqual(image_output[1], "latest")
        self.assertEqual(image_output[5], "docker.io")

        # loaded image
        image_output = self._image_in_output_of_images_command(is_centralized_repository, False, "load/library/loaded_image", "latest", "")
        self.assertEqual(image_output[0], "load/library/loaded_image")
        self.assertEqual(image_output[1], "latest")
        self.assertEqual(image_output[2], "501d1a8f0487")
        self.assertEqual(image_output[5], "load")
        self.assertTrue(re.match(r"1\.[89]\dMB", image_output[4]))

    def _header_in_output_of_images_command(self, is_centralized_repository, print_digests=False):
        return util.get_images_command_output(is_centralized_repository, print_digests)[0]

    def _image_in_output_of_images_command(self, is_centralized_repository, print_digests, image, tag, digest):
        output = util.get_images_command_output(is_centralized_repository, print_digests)
        for line in output:
            if print_digests:
                if line[0] == image and line[1] == tag and line[2] == digest:
                    return line
            else:
                if line[0] == image and line[1] == tag:
                    return line
