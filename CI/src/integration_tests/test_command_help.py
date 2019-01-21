import unittest
import subprocess
import re


import common.util as util


class TestCommandHelp(unittest.TestCase):
    def test_command_help(self):
        self._test_command_help(command=["sarus", "--help"])
        self._test_command_help(command=["sarus", "help"])

    def _test_command_help(self, command):
        out = subprocess.check_output(command)
        lines = util.command_output_without_trailing_new_lines(out)
        self.assertGreater(len(lines), 1)
        self.assertEqual(lines[0], "Usage: sarus COMMAND")
