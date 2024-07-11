# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import common.util as util
import pytest
import psutil
import subprocess
import time
import unittest


class TestCommandPs(unittest.TestCase):

    CONTAINER_IMAGE = util.ALPINE_IMAGE

    @classmethod
    def setUpClass(cls):
        try:
            util.pull_image_if_necessary(
                is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        except Exception as e:
            print(e)

    def test_ps_command_is_defined(self):
        try:
            subprocess.check_output(["sarus", "help", "ps"])
        except subprocess.CalledProcessError as _:
            self.fail("Can't execute command `sarus ps`")

    def test_ps_shows_running_container(self):
        sarus_process = psutil.Popen(["sarus", "run", "--name", "test_container", self.CONTAINER_IMAGE, "sleep", "5"])
        time.sleep(2)
        output = subprocess.check_output(["sarus", "ps"]).decode()
        self.assertGreater(len(output.splitlines()),1)
        self.assertTrue(any(["test_container" in line for line in output.splitlines()]))

    @pytest.mark.skip("This test requires to run with a different identity")
    def test_ps_hides_running_container_from_other_users(self):
        sarus_process = psutil.Popen(["sarus", "run", "--name", "test_container", self.CONTAINER_IMAGE, "sleep", "5"])
        time.sleep(2)
        output = subprocess.check_output(["sarus", "ps"], user="janedoe").decode()
        
        try:
            self.assertEqual(len(output.splitlines()), 1)
        except AssertionError:
            self.assertGreater(len(output.splitlines()), 1)
            self.assertFalse(any(["test_container" in line for line in output.splitlines()]))
