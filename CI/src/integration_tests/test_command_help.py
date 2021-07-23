# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess


import common.util as util


class TestCommandHelp(unittest.TestCase):
    def test_command_help(self):
        self._test_command_help(command=["sarus", "--help"])
        self._test_command_help(command=["sarus", "help"])

    def _test_command_help(self, command):
        out = subprocess.check_output(command).decode()
        lines = util.command_output_without_trailing_new_lines(out)
        self.assertGreater(len(lines), 1)
        self.assertEqual(lines[0], "Usage: sarus COMMAND")
