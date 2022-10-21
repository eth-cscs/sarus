# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import re

import common.util as util


class TestCapabilities(unittest.TestCase):

    CONTAINER_IMAGE = util.ALPINE_IMAGE

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)

    def test_linux_capabilities_inside_container(self):
        output = util.run_command_in_container(is_centralized_repository=False,
                                      image=self.CONTAINER_IMAGE,
                                      command=["cat", "/proc/self/status"])
        assert any(re.match(r'^CapInh:\t+0+', line) for line in output)
        assert any(re.match(r'^CapPrm:\t+0+', line) for line in output)
        assert any(re.match(r'^CapEff:\t+0+', line) for line in output)
        assert any(re.match(r'^CapBnd:\t+0+', line) for line in output)
        assert any(re.match(r'^CapAmb:\t+0+', line) for line in output)
