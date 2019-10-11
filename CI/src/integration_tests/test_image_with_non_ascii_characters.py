# -*- coding: utf-8 -*-
#
# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestImageWithNonAsciiCharacters(unittest.TestCase):
    """
    These tests simply pull, run and remove an image with a folder
    and a file whose names have non-ascii characters.
    """

    _IMAGE_NAME = "ethcscs/dockerfiles:image-with-non-ascii-characters"

    def test_image_with_non_ascii_characters(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        output = util.run_command_in_container(  is_centralized_repository=False,
                                                        image=self._IMAGE_NAME,
                                                        command=["ls", "/földèr"])
        # the command's output might also contain escape sequences (e.g. color codes), let's ignore
        # the escape characters and let's just check that the output contains the expected string
        output_contains_expected_string = output[0].find("filé") != -1
        output_contains_expected_string = output[0].find("ファイル") != -1
        self.assertTrue(output_contains_expected_string)
