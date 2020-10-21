# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestImageWithMaxPathLength(unittest.TestCase):
    """
    These tests simply pull and run an image with a very long path name inside.

    Ideally the very long path name should be 4096 bytes long (max path length on Linux).
    However, in practice it is shorter because it gets trunkated at container build time.
    """

    _IMAGE_NAME = "ethcscs/dockerfiles:image-with-max-path-length"

    def test_image_with_max_path_length(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)
        prettyname = util.run_image_and_get_prettyname(is_centralized_repository=False, image=self._IMAGE_NAME)
        assert prettyname == "Alpine Linux"
