# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import re

import common.util as util


class TestHookStdoutStderr(unittest.TestCase):
    """
    This test checks that a hook can successfully log messages to stdout and stderr.
    """

    CONTAINER_IMAGE = util.ALPINE_IMAGE
    _OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/stdout_stderr_test.json"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        cls._enable_hook()

    @classmethod
    def tearDownClass(cls):
        cls._disable_hook()

    @classmethod
    def _enable_hook(cls):
        hook = {
            "version": "1.0.0",
            "hook": {
                "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/stdout_stderr_test_hook",
            },
            "when": {
                "always": True
            },
            "stages": ["prestart"]
        }

        util.create_hook_file(hook, cls._OCIHOOK_CONFIG_FILE)

    @classmethod
    def _disable_hook(cls):
        subprocess.call(["sudo", "rm", cls._OCIHOOK_CONFIG_FILE])

    def test_stdout(self):
        # DEBUG log level
        expected = [
            r"^hook's stdout$",
            r"^\[\d+.\d+\] \[.+\] \[stdout_stderr_test_hook\] \[DEBUG\] hook's DEBUG log message$",
            r"^\[\d+.\d+\] \[.+\] \[stdout_stderr_test_hook\] \[INFO\] hook's INFO log message$",
        ]
        stdout = self._get_sarus_stdout("--debug")
        self._check_output_matches(stdout, expected)

        # INFO log level
        expected = [
            r"^hook's stdout$",
            r"^\[\d+.\d+\] \[.+\] \[stdout_stderr_test_hook\] \[INFO\] hook's INFO log message$",
        ]
        stdout = self._get_sarus_stdout("--verbose")
        self._check_output_matches(stdout, expected)

        # WARNING log level
        expected = [
            r"^hook's stdout$",
        ]
        stdout = self._get_sarus_stdout()
        self._check_output_matches(stdout, expected)

    def test_stderr(self):
        expected = [
            r"^hook's stderr$",
            r"^\[\d+.\d+\] \[.+\] \[stdout_stderr_test_hook\] \[WARN\] hook's WARN log message$",
            r"^\[\d+.\d+\] \[.+\] \[stdout_stderr_test_hook\] \[ERROR\] hook's ERROR log message$",
        ]
        stderr = util.get_sarus_error_output(["sarus", "run", self.CONTAINER_IMAGE, "true"],
                                             fail_expected=False)
        self._check_output_matches(stderr.split('\n'), expected)

    def _get_sarus_stdout(self, verbose_option=None):
        command = ["sarus", "run", self.CONTAINER_IMAGE, "true"]
        if verbose_option is not None:
            command.insert(1, verbose_option)
        return util.get_trimmed_output(command)

    def _check_output_matches(self, actuals, expecteds):
        assert len(expecteds) > 0
        i = 0

        for actual in actuals:
            if re.match(expecteds[i], actual):
                i += 1
            if i == len(expecteds):
                return

        message = "Expression '" + expecteds[i] + "' didn't match any line in Sarus's output:\n\n" + str(actuals)
        assert False, message
