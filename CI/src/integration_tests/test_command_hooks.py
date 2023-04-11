# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import subprocess


import common.util as util


class TestCommandHooks(unittest.TestCase):
    """
    These tests verify the outputs "hooks" command
    """

    def test_command_hooks(self):
        expected_header = ["NAME", "PATH", "STAGES"]
        actual_header = self._header_in_output_of_hooks_command()
        self.assertEqual(actual_header, expected_header)

        hook_output = self._hook_in_output_of_hooks_command("07-ssh-hook")
        self.assertEqual(hook_output[1], "/opt/sarus/default/bin/ssh_hook")
        self.assertEqual(hook_output[2], "prestart")

        hook_output = self._hook_in_output_of_hooks_command("09-slurm-global-sync-hook")
        self.assertEqual(hook_output[1], "/opt/sarus/default/bin/slurm_global_sync_hook")
        self.assertEqual(hook_output[2], "prestart")

    def test_command_hooks_mpi(self):
        expected_header = ["NAME", "MPI", "TYPE"]
        actual_header = self._header_in_output_of_hooks_command(print_mpi_hooks=True)
        self.assertEqual(actual_header, expected_header)

        hook_config_paths = [os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/050-mpi0-hook.json",
                             os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/051-mpi1-hook.json"]

        with util.temporary_hook_files((self._generate_mpi_hook_config("mpi0"), hook_config_paths[0]),
                                       (self._generate_mpi_hook_config("mpi1"), hook_config_paths[1])):
            with util.custom_sarus_json({"defaultMPIType": "mpi0"}):
                hook_output = self._hook_in_output_of_hooks_command("050-mpi0-hook", print_mpi_hooks=True)
                self.assertEqual(hook_output, ['050-mpi0-hook', '^mpi0$', '(default)'])

                hook_output = self._hook_in_output_of_hooks_command("051-mpi1-hook", print_mpi_hooks=True)
                self.assertEqual(hook_output, ['051-mpi1-hook', '^mpi1$'])

    def _header_in_output_of_hooks_command(self, print_mpi_hooks=False):
        return util.get_hooks_command_output(print_mpi_hooks)[0]

    def _hook_in_output_of_hooks_command(self, hook_name, print_mpi_hooks=False):
        output = util.get_hooks_command_output(print_mpi_hooks=print_mpi_hooks)
        for line in output:
            if line[0] == hook_name:
                return line

    def _generate_mpi_hook_config(self, mpi_type):
        config = {
            "version": "1.0.0",
            "hook": {
                "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/mpi_hook",
                "env": []
            },
            "when": {
                "annotations": {
                    "^com.hooks.mpi.enabled$": "^true$",
                    "^com.hooks.mpi.type": f"^{mpi_type}$"
                }
            },
            "stages": ["prestart"]
        }
        return config
