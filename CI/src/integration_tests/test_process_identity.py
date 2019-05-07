# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import os
import unittest

import common.util as util


class TestProcessIdentity(unittest.TestCase):
    """
    These tests verify that the identity of the host process (represented by uid, gid and supplementary gids)
    is correctly preserved within the container.
    Notice that here we refer specifically to the ids of the *process*, not those found inside /etc/groups.
    """

    _IMAGE_NAME = "alpine"

    def test_process_identity(self):
        host_uid = os.getuid()
        host_gid = os.getgid()
        host_supplementary_gids = os.getgroups()

        # Retrieve ids from container
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)

        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["id", "-u"])
        container_uid = int(output[0])

        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["id", "-g"])
        container_gid = int(output[0])

        output = util.run_command_in_container(is_centralized_repository=False,
                                               image=self._IMAGE_NAME,
                                               command=["id", "-G"])
        container_supplementary_gids = [int(group) for group in output[0].split()]

        # Checks
        self.assertEqual(host_uid, container_uid)
        self.assertEqual(host_gid, container_gid)
        self.assertEqual(host_supplementary_gids, container_supplementary_gids)


if __name__ == "__main__":

    TestProcessIdentity().test_process_identity()
