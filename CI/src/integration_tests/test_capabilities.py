# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import pexpect

import common.util as util


class TestCapabilities(unittest.TestCase):

    _CONTAINER_IMAGE = "quay.io/ethcscs/alpine:3.14"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls._CONTAINER_IMAGE)

    def test_linux_capabilities_inside_container(self):
        pex = pexpect.spawnu("sarus run %s cat /proc/self/status | grep Cap" % self._CONTAINER_IMAGE)
        pex.expect(r'CapInh:\s+0+')
        pex.expect(r'CapPrm:\s+0+')
        pex.expect(r'CapEff:\s+0+')
        pex.expect(r'CapBnd:\s+0+')
