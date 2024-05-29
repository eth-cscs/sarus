# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os

import common.util as util


def generate_hook_config():
    hook_config = {
        "version": "1.0.0",
        "hook": {
            "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/ssh_hook",
            "env": [
                "HOOK_BASE_DIR=/home",
                "PASSWD_FILE=" + os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/passwd",
                "DROPBEAR_DIR=" + os.environ["CMAKE_INSTALL_PREFIX"] + "/dropbear",
                "SERVER_PORT_DEFAULT=15263"
            ],
            "args": [
                "ssh_hook",
                "start-ssh-daemon"
            ]
        },
        "when": {
            "annotations": {
                "^com.hooks.ssh.enabled$": "^true$"
            }
        },
        "stages": ["createRuntime"]
    }
    return hook_config


class TestSshHook(unittest.TestCase):
    """
    These tests verify that the optional mount of ~/.ssh of the Ssh hook works correctly.
    """
    OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/ssh_hook.json"
    CONTAINER_IMAGE = "quay.io/ethcscs/ubuntu:20.04"

    _CI_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image=cls.CONTAINER_IMAGE)        

    def test_mount_host_dotssh_is_mounted_as_overlay_by_default(self):
        with util.temporary_hook_files((generate_hook_config(), self.OCIHOOK_CONFIG_FILE)):
            util.generate_ssh_keys()
            mounts = util.run_command_in_container(is_centralized_repository=False,
                                          image=self.CONTAINER_IMAGE,
                                          options_of_run_command=["--ssh"],
                                          command=["mount"])[1:]
            assert any(("/.ssh" in mount for mount in mounts))

    def test_mount_host_dotssh_is_not_mounted_if_env_var_is_true(self):
        hook_config = generate_hook_config()
        hook_config["hook"]["env"] += ["OVERLAY_MOUNT_HOME_SSH=True"]
        with util.temporary_hook_files((hook_config, self.OCIHOOK_CONFIG_FILE)):
            util.generate_ssh_keys()
            mounts = util.run_command_in_container(is_centralized_repository=False,
                                          image=self.CONTAINER_IMAGE,
                                          options_of_run_command=["--ssh"],
                                          command=["mount"])[1:]
            assert any(("/.ssh" in mount for mount in mounts))

    def test_mount_host_dotssh_is_not_mounted_if_env_var_is_false(self):
        hook_config = generate_hook_config()
        hook_config["hook"]["env"] += ["OVERLAY_MOUNT_HOME_SSH=False"]
        with util.temporary_hook_files((hook_config, self.OCIHOOK_CONFIG_FILE)):
            util.generate_ssh_keys()
            mounts = util.run_command_in_container(is_centralized_repository=False,
                                          image=self.CONTAINER_IMAGE,
                                          options_of_run_command=["--ssh"],
                                          command=["mount"])[1:]
            assert not any(("/.ssh" in mount for mount in mounts))
