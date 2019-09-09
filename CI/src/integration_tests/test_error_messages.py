# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
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

        command = ["sarus", "help", "invalid-command"]
        expected_message = "'invalid-command' is not a Sarus command\nSee 'sarus help'"
        self._check(command, expected_message)

        command = ["sarus", "help", "run", "extra-argument"]
        expected_message = "Command 'help' doesn't support extra argument 'extra-argument'\nSee 'sarus help help'"
        self._check(command, expected_message)

    def test_command_images(self):
        command = ["sarus", "images", "--invalid-option"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help images'"
        self._check(command, expected_message)

        command = ["sarus", "images", "extra-argument"]
        expected_message = "Command 'images' doesn't support extra argument 'extra-argument'\nSee 'sarus help images'"
        self._check(command, expected_message)

    def test_command_load(self):
        command = ["sarus", "load", "--invalid-option", "archive.tar", "imageid"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar"]
        expected_message = "Bad number of arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "imageid", "extra-argument"]
        expected_message = "Bad number of arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "--temp-dir=/invalid-dir", "archive.tar", "imageid"]
        expected_message = "Invalid temporary directory \"/invalid-dir\""
        self._check(command, expected_message)

    def test_command_pull(self):
        command = ["sarus", "pull", "--invalid-option", "alpine:latest"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", "alpine", "--invalid-option"]
        expected_message = ("Invalid image ID [\"alpine\", \"--invalid-option\"]\n"
                           "The image ID is expected to be a single token without options")
        self._check(command, expected_message)

        command = ["sarus", "pull", "///"]
        expected_message = ("Invalid image ID '///'")
        self._check(command, expected_message)

        command = ["sarus", "pull", "invalid-image-1kds710dkj"]
        expected_message = ("Failed to pull image 'index.docker.io/library/invalid-image-1kds710dkj:latest'\nIs the image ID correct?")
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

        command = ["sarus", "version", "extra-argument"]
        expected_message = "Command 'version' doesn't support extra argument 'extra-argument'\nSee 'sarus help version'"
        self._check(command, expected_message)

    def _check(self, command, expected_message):
        actual_message = self._get_sarus_error_output(command)
        self.assertEqual(actual_message, expected_message)

    def _get_sarus_error_output(self, command):
        with open(os.devnull, 'wb') as devnull:
            proc = subprocess.Popen(command,
                                    stdout=devnull,
                                    stderr=subprocess.PIPE)
            stderr = proc.communicate()[1]
            if proc.returncode == 0:
                raise Exception("Sarus didn't generate any error, but at least one was expected.")
            else:
                stderr_without_trailing_whitespaces = stderr.rstrip()
                return stderr_without_trailing_whitespaces


if __name__ == "__main__":
    unittest.main()
