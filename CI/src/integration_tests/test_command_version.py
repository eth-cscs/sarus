# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestCommandVersion(unittest.TestCase):
    def test_command_version(self):
        self._test_command_version(command=["sarus", "--version"])
        self._test_command_version(command=["sarus", "version"])

    def _test_command_version(self, command):
        lines = util.get_trimmed_output(command)

        assert len(lines) == 1
        version_obtained = lines[0]

        # This test is intended to test either an official release or a development version, both of which are always
        # git describable. See scripts in CI folder for the Sarus source location
        sarus_source_location = "/sarus-source"
        version_expected = util.get_trimmed_output(["git", "describe", "--tags", "--dirty", "--always"],
                                                   cwd=sarus_source_location)[0]
        assert version_obtained == version_expected
