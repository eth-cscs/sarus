import unittest
import pexpect

import common.util as util


class TestCapabilities(unittest.TestCase):

    _CONTAINER_IMAGE = "alpine:3.8"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls._CONTAINER_IMAGE)

    def test_linux_capabilities_inside_container(self):
        pex = pexpect.spawnu("sarus run %s cat /proc/self/status | grep Cap" % self._CONTAINER_IMAGE)
        pex.expect(u'CapInh:\s+0+')
        pex.expect(u'CapPrm:\s+0+')
        pex.expect(u'CapEff:\s+0+')
        pex.expect(u'CapBnd:\s+0+')
