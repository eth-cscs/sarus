# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess

import common.util as util


class TestErrorMessages(unittest.TestCase):
    """
    These tests verify that Sarus generates user-friendly error messages
    when the user's input is invalid.
    """

    def test_sarus(self):
        command = ["sarus", "--invalid-option"]
        expected_message = "Error while parsing the command line: unrecognised option '--invalid-option'"
        self._check(command, expected_message)

        command = ["sarus", "invalid-command"]
        expected_message = "'invalid-command' is not a Sarus command\nSee 'sarus help'"
        self._check(command, expected_message)

    def test_command_help(self):
        command = ["sarus", "help", "--invalid-option"]
        expected_message = "Command 'help' doesn't support options"
        self._check(command, expected_message)

    def test_command_help_on_command(self):
        command = ["sarus", "help", "--invalid-option", "run"]
        expected_message = "Command 'help' doesn't support options"
        self._check(command, expected_message)

    def test_command_sshkeygen(self):
        command = ["sarus", "ssh-keygen", "--invalid-option"]
        expected_message = "Command 'ssh-keygen' doesn't support options\nSee 'sarus help ssh-keygen'"
        self._check(command, expected_message)

        command = ["sarus", "ssh-keygen", "additional_argument"]
        expected_message = "Command 'ssh-keygen' doesn't support additional arguments\nSee 'sarus help ssh-keygen'"
        self._check(command, expected_message)

    def test_command_version(self):
        command = ["sarus", "version", "--invalid-option"]
        expected_message = "Command 'version' doesn't support options\nSee 'sarus help version'"
        self._check(command, expected_message)

        command = ["sarus", "version", "additional_argument"]
        expected_message = "Command 'version' doesn't support additional arguments\nSee 'sarus help version'"
        self._check(command, expected_message)

    def _check(self, command, expected_message):
        actual_message = self._get_sarus_error_output(command)
        self.assertEqual(actual_message, expected_message)

    def _get_sarus_error_output(self, command):
        try:
            subprocess.check_output(command, stderr=subprocess.STDOUT)
            raise Exception("Sarus didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            output_without_trailing_whitespaces = ex.output.rstrip()
            return output_without_trailing_whitespaces


if __name__ == "__main__":
    unittest.main()
