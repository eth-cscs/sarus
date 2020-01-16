# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import json

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

    def test(self):
        stdout = self._get_sarus_stdout()
        self.assertEqual("hook's stdout", stdout)

        stderr = self._get_sarus_stderr()
        self.assertEqual("hook's stderr", stderr)

    def _get_sarus_stdout(self):
        stdout = subprocess.check_output(["sarus", "run", "alpine:latest", "true"]).decode()
        return stdout.rstrip() # remove trailing whitespaces

    def _get_sarus_stderr(self):
        with open(os.devnull, 'wb') as devnull:
            proc = subprocess.run(["sarus", "run", "alpine:latest", "true"],
                                  stdout=devnull,
                                  stderr=subprocess.PIPE)
        if proc.returncode != 0:
            self.fail("An error occurred while executing Sarus")

        return proc.stderr.rstrip().decode()
