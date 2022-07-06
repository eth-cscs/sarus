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
        try:
            out = subprocess.check_output(command).decode()
            lines = util.command_output_without_trailing_new_lines(out)
        except Exception:
            assert False, "failed to get sarus version from sarus"

        assert len(lines) == 1
        version_obtained = lines[0]

        # This test is intended to test either an official release or a development version, both of which are always git describable.
        try:
            # See scripts in CI folder
            sarus_source_location = "/sarus-source"
            out = subprocess.check_output(["git", "describe", "--tags", "--dirty", "--always"], cwd=sarus_source_location).decode()
            version_expected = util.command_output_without_trailing_new_lines(out)[0]
        except Exception as e:
            assert False, f"failed to get sarus version from git. {e}"

        assert version_obtained == version_expected