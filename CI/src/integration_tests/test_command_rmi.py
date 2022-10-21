# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestCommandRmi(unittest.TestCase):
    """
    These tests verify that the images are correctly removed.
    """

    def test_command_rmi_with_local_repository(self):
        self._test_command_rmi(is_centralized_repository=False)

    def test_command_rmi_with_centralized_repository(self):
        self._test_command_rmi(is_centralized_repository=True)

    def test_command_rmi_by_digest(self):
        self._test_command_rmi(is_centralized_repository=False,
                               image="quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                               expected_string="quay.io/ethcscs/alpine:<none>@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

    def test_command_rmi_by_tag_and_digest(self):
        self._test_command_rmi(is_centralized_repository=False,
                               image="quay.io/ethcscs/alpine:3.14@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                               expected_string="quay.io/ethcscs/alpine:<none>@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

    def test_command_rmi_without_backing_file(self):
        self._test_command_rmi(is_centralized_repository=False, remove_backing_file=True)

    def _test_command_rmi(self, is_centralized_repository, image=util.ALPINE_IMAGE,
                          expected_string=None, remove_backing_file=False):
        expected_image = expected_string if expected_string else image

        util.pull_image_if_necessary(is_centralized_repository, image)
        self.assertTrue(util.is_image_available(is_centralized_repository, expected_image))

        if remove_backing_file:
            util.remove_image_backing_file(image)

        util.remove_image_if_necessary(is_centralized_repository, image)
        self.assertFalse(util.is_image_available(is_centralized_repository, expected_image))
