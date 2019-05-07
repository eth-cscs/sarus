# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestCommandRun(unittest.TestCase):
    """
    These tests verify that the available images are run correctly,
    i.e. the Linux's pretty name inside the container is correct.
    """

    def test_command_run_with_local_repository(self):
        self._test_command_run(is_centralized_repository=False)

    def test_command_run_with_centralized_repository(self):
        self._test_command_run(is_centralized_repository=True)

    def _test_command_run(self, is_centralized_repository):
        util.pull_image_if_necessary(is_centralized_repository, "library/alpine:3.8")
        util.pull_image_if_necessary(is_centralized_repository, "library/debian:jessie")
        util.pull_image_if_necessary(is_centralized_repository, "library/debian:stretch")
        util.pull_image_if_necessary(is_centralized_repository, "library/centos:7")

        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "library/alpine:3.8"),
            "Alpine Linux")
        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "library/debian:jessie"),
            "Debian GNU/Linux 8 (jessie)")
        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "library/debian:stretch"),
            "Debian GNU/Linux 9 (stretch)")
        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "library/centos:7"),
            "CentOS Linux")
