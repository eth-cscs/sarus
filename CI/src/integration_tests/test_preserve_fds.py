# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import os
import re
import unittest

import common.util as util


class TestPreserveFDS(unittest.TestCase):
    """
    This test verifies that Sarus is able to instruct the low-level OCI runtime to preserve into the container the
    additional file descriptors (i.e. not including stdio) available to the calling process.
    """

    _IMAGE_NAME = "debian:stretch"

    def test_preserve_fds(self):
        test_file_1 = open("/tmp/test1", "w")
        test_file_2 = open("/tmp/test2", "w")
        test_file_3 = open("/tmp/test3", "w")
        test_file_4 = open("/tmp/test4", "w")

        host_fds = os.listdir("/proc/self/fd")

        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)

        # Check without LISTEN_FDS
        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["dir", "/proc/self/fd"])
        container_fds = re.split("\W+", output[0])

        self.assertEqual(len(host_fds), len(container_fds))

        # Check with LISTEN_FDS
        os.environ["LISTEN_FDS"] = "3"
        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["dir", "/proc/self/fd"])
        container_fds = re.split("\W+", output[0])

        self.assertEqual(len(host_fds)-3, len(container_fds))

        test_file_1.close()
        test_file_2.close()
        test_file_3.close()
        test_file_4.close()