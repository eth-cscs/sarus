# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import common.util as util
import psutil
import subprocess
import time
import unittest

from pathlib import Path


class TestCommandKill(unittest.TestCase):

    CONTAINER_IMAGE = util.ALPINE_IMAGE

    @classmethod
    def setUpClass(cls):
        try:
            util.pull_image_if_necessary(
                is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        except Exception as e:
            print(e)

    def test_kill_command_is_defined(self):
        try:
            subprocess.check_output(["sarus", "help", "kill"])
        except subprocess.CalledProcessError as _:
            self.fail("Can't execute command `sarus kill`")


    def test_kill_deletes_running_container(self):
        sarus_process = psutil.Popen(["sarus", "run", "--name", "test_container", self.CONTAINER_IMAGE, "sleep", "5"])
        
        time.sleep(2)
        sarus_children = sarus_process.children(recursive=True)
        self.assertGreater(len(sarus_children), 0, "At least the sleep process must be there")
        
        psutil.Popen(["sarus", "kill", "test_container"])
        time.sleep(1)
        
        self.assertFalse(any([p.is_running() for p in sarus_children]),
                         "Sarus child processes were not cleaned up")
        self.assertFalse(list(Path("/sys/fs/cgroup/cpuset").glob("test_container")),
                         "Cgroup subdir was not cleaned up")
