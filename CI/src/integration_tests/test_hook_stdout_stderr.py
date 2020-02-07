# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import json
import re

import common.util as util


class TestHookStdoutStderr(unittest.TestCase):
    """
    This test checks that a hook can successfully log messages to stdout and stderr.
    """

    _SARUS_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image="alpine:latest")
        cls._enable_stdout_stderr_test_hook()

    @classmethod
    def tearDownClass(cls):
        cls._disable_stdout_stderr_test_hook()

    @classmethod
    def _enable_stdout_stderr_test_hook(cls):
        # make backup of config file
        subprocess.call(["sudo", "cp", cls._SARUS_CONFIG_FILE, cls._SARUS_CONFIG_FILE + ".bak"])

        # Build OCIHooks object
        hook = dict()
        hook["path"] = os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/stdout_stderr_test_hook"
        hooks = dict()
        hooks["prestart"] = [hook]

        # Modify config file
        with open(cls._SARUS_CONFIG_FILE, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["OCIHooks"] = hooks
        data = json.dumps(config)
        with open("sarus.json.tmp", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.tmp", cls._SARUS_CONFIG_FILE])

    @classmethod
    def _disable_stdout_stderr_test_hook(cls):
        subprocess.call(["sudo", "mv", cls._SARUS_CONFIG_FILE + ".bak", cls._SARUS_CONFIG_FILE])

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
        stderr = self._get_sarus_stderr()
        self._check_output_matches(stderr, expected)

    def _get_sarus_stdout(self, verbose_option=None):
        command = ["sarus", "run", "alpine:latest", "true"]
        if verbose_option is not None:
            command = command[0:1] + [verbose_option] + command[1:]
        stdout = subprocess.check_output(command).decode()
        return stdout.rstrip().split('\n') # remove trailing whitespaces

    def _get_sarus_stderr(self):
        command = ["sarus", "run", "alpine:latest", "true"]
        with open(os.devnull, 'wb') as devnull:
            proc = subprocess.run(command,
                                  stdout=devnull,
                                  stderr=subprocess.PIPE)
        assert proc.returncode == 0, "An error occurred while executing Sarus"

        return proc.stderr.decode().rstrip().split('\n')

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
