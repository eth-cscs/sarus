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
        test_file = open("/tmp/test1", "w+")
        test_fd = test_file.fileno()

        # Test preservation of PMI_FD inside the container
        host_environment = os.environ.copy()
        host_environment["PMI_FD"] = str(test_fd)

        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)

        command = ["bash", "-c", "echo fd-preservation-test >&${PMI_FD}"]
        sarus_options = ["--mount=type=bind,source=/tmp,destination=/tmp"]

        util.run_command_in_container(is_centralized_repository=False,
                                      image=self._IMAGE_NAME,
                                      command=command,
                                      options_of_run_command=sarus_options,
                                      environment=host_environment)

        test_file.seek(0)
        self.assertEqual(test_file.readline()[:-1], "fd-preservation-test")

        test_file.close()