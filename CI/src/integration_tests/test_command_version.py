import unittest
import subprocess
import re


import common.util as util


class TestCommandVersion(unittest.TestCase):
    def test_command_version(self):
        self._test_command_version(command=["sarus", "--version"])
        self._test_command_version(command=["sarus", "version"])

    def _test_command_version(self, command):
        out = subprocess.check_output(command)
        lines = util.command_output_without_trailing_new_lines(out)
        self.assertEqual(len(lines), 1)
        self.assertTrue(re.match(r"^\d+\.\d+\.\d+(-.+)*$", lines[0]) is not None)
