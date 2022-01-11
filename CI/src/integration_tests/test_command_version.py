# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import re


import common.util as util


class TestCommandVersion(unittest.TestCase):
    def test_command_version(self):
        self._test_command_version(command=["sarus", "--version"])
        self._test_command_version(command=["sarus", "version"])

    def _test_command_version(self, command):
        out = subprocess.check_output(command).decode()
        lines = util.command_output_without_trailing_new_lines(out)
        assert len(lines) == 1
        assert re.match(r"^\d+\.\d+\.\d+(-.+)*$", lines[0]) is not None \
               or lines[0] == "VERSION-NOT-AVAILABLE"
