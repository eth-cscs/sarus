# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os

import common.util as util


class TestCommandHooks(unittest.TestCase):
    """
    These tests verify the outputs of the "hooks" command
    """

    def test_command_hooks(self):
        expected_header = ["NAME", "PATH", "STAGES"]
        actual_header = header_in_output_of_hooks_command()
        self.assertEqual(actual_header, expected_header)

        with util.temporary_hook_files((generate_dummy_hook_config(),
                                        os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/01-dummy-hook.json")):
            hook_output = hook_in_output_of_hooks_command("01-dummy-hook")
            self.assertEqual(hook_output[1], "/opt/sarus/default/bin/dummy_hook")
            self.assertEqual(hook_output[2], "prestart")

    def test_command_hooks_mpi(self):
        expected_header = ["NAME", "MPI", "TYPE"]
        actual_header = header_in_output_of_hooks_command(print_mpi_hooks=True)
        self.assertEqual(actual_header, expected_header)

        hook_config_paths = [os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/050-mpi0-hook.json",
                             os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/051-mpi1-hook.json"]

        with util.temporary_hook_files((generate_mpi_hook_config("mpi0"), hook_config_paths[0]),
                                       (generate_mpi_hook_config("mpi1"), hook_config_paths[1])):
            with util.custom_sarus_json({"defaultMPIType": "mpi0"}):
                hook_output = hook_in_output_of_hooks_command("050-mpi0-hook", print_mpi_hooks=True)
                self.assertEqual(hook_output, ['050-mpi0-hook', '^mpi0$', '(default)'])

                hook_output = hook_in_output_of_hooks_command("051-mpi1-hook", print_mpi_hooks=True)
                self.assertEqual(hook_output, ['051-mpi1-hook', '^mpi1$'])


def header_in_output_of_hooks_command(print_mpi_hooks=False):
    return util.get_hooks_command_output(print_mpi_hooks)[0]


def hook_in_output_of_hooks_command(hook_name, print_mpi_hooks=False):
    output = util.get_hooks_command_output(print_mpi_hooks=print_mpi_hooks)
    for line in output:
        if line[0] == hook_name:
            return line


def generate_dummy_hook_config():
    config = {
        "version": "1.0.0",
        "hook": {
            "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/dummy_hook",
            "env": []
        },
        "when": {
            "always": True
        },
        "stages": ["prestart"]
    }
    return config


def generate_mpi_hook_config(mpi_type):
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
