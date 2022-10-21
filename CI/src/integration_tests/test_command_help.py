# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestCommandHelp(unittest.TestCase):
    def test_command_help(self):
        self._test_command_help(command=["sarus", "--help"])
        self._test_command_help(command=["sarus", "help"])

    def _test_command_help(self, command):
        lines = util.get_trimmed_output(command)
        self.assertGreater(len(lines), 1)
        self.assertEqual(lines[0], "Usage: sarus COMMAND")
