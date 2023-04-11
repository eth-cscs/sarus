# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import shutil

import common.util as util


class TestErrorMessages(unittest.TestCase):
    """
    These tests verify that Sarus generates user-friendly error messages
    when the user's input is invalid.
    """

    DEFAULT_IMAGE = util.ALPINE_IMAGE

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.DEFAULT_IMAGE)

    def test_sarus(self):
        command = ["sarus", "--invalid-option"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help'"
        self._check(command, expected_message)

        command = ["sarus", "invalid-command"]
        expected_message = "'invalid-command' is not a Sarus command\nSee 'sarus help'"
        self._check(command, expected_message)

        command = ["sarus", "--verbose", "---run"]
        expected_message = "'---run' is not a Sarus command"
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
        expected_message = "Too many arguments for command 'help'\nSee 'sarus help help'"
        self._check(command, expected_message)

    def test_command_images(self):
        command = ["sarus", "images", "--invalid-option"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help images'"
        self._check(command, expected_message)

        command = ["sarus", "images", "extra-argument"]
        expected_message = "Too many arguments for command 'images'\nSee 'sarus help images'"
        self._check(command, expected_message)

    def test_command_load(self):
        command = ["sarus", "load", "--invalid-option", "archive.tar", "imageRef"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load"]
        expected_message = "Too few arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar"]
        expected_message = "Too few arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "imageRef", "extra-argument"]
        expected_message = "Too many arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "alpine", "--invalid-option"]
        expected_message = "Too many arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "///"]
        expected_message = "Invalid image reference '///'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "target-image@sha256:778fdd9f62a6d7c0e53a97489ab3db17738bc5c1acf09a18738a2a674025eae6"]
        expected_message = "Destination image reference must not contain a digest when loading the image from a file\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "target-image:latest@sha256:778fdd9f62a6d7c0e53a97489ab3db17738bc5c1acf09a18738a2a674025eae6"]
        expected_message = "Destination image reference must not contain a digest when loading the image from a file\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "--temp-dir=/invalid-dir", "archive.tar", self.DEFAULT_IMAGE]
        expected_message = "Invalid temporary directory \"/invalid-dir\""
        self._check(command, expected_message)

    def test_command_pull(self):
        command = ["sarus", "pull", "--invalid-option", self.DEFAULT_IMAGE]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull"]
        expected_message = "Too few arguments for command 'pull'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", self.DEFAULT_IMAGE, "extra-argument"]
        expected_message = "Too many arguments for command 'pull'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", self.DEFAULT_IMAGE, "--invalid-option"]
        expected_message = "Too many arguments for command 'pull'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", "--temp-dir=/invalid-dir", self.DEFAULT_IMAGE]
        expected_message = "Invalid temporary directory \"/invalid-dir\""
        self._check(command, expected_message)

        command = ["sarus", "pull", "///"]
        expected_message = "Invalid image reference '///'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", "invalid-image-1kds710dkj"]
        expected_message = ("Failed to pull image 'docker.io/library/invalid-image-1kds710dkj:latest'"
                            "\nError reading manifest from registry."
                            "\nThe image may be private or not present in the remote registry."
                            "\nDid you perform a login with the proper credentials?"
                            "\nSee 'sarus help pull' (--login option)")
        self._check(command, expected_message)

        command = ["sarus", "pull", "quay.io/ethcscs/invalid-image-1kds710dkj"]
        expected_message = ("Failed to pull image 'quay.io/ethcscs/invalid-image-1kds710dkj:latest'"
                            "\nError reading manifest from registry."
                            "\nThe image may be private or not present in the remote registry."
                            "\nDid you perform a login with the proper credentials?"
                            "\nSee 'sarus help pull' (--login option)")
        self._check(command, expected_message)

        command = ["sarus", "pull", "ethcscs/private-example"]
        expected_message = ("Failed to pull image 'docker.io/ethcscs/private-example:latest'"
                            "\nError reading manifest from registry."
                            "\nThe image may be private or not present in the remote registry."
                            "\nDid you perform a login with the proper credentials?"
                            "\nSee 'sarus help pull' (--login option)")
        self._check(command, expected_message)

        command = ["sarus", "pull", "quay.io/ethcscs/private-example"]
        expected_message = ("Failed to pull image 'quay.io/ethcscs/private-example:latest'"
                            "\nError reading manifest from registry."
                            "\nThe image may be private or not present in the remote registry."
                            "\nDid you perform a login with the proper credentials?"
                            "\nSee 'sarus help pull' (--login option)")
        self._check(command, expected_message)

    def test_command_pull_authentication_options(self):
        command = ["bash", "-c", "printf 'invalid-username\ninvalid-password' | sarus pull --login quay.io/ethcscs/private-example"]
        expected_message = ("Failed to pull image 'quay.io/ethcscs/private-example:latest'"
                            "\nUnable to retrieve auth token: invalid username or password provided.")
        self._check(command, expected_message)

        command = ["sarus", "pull", "--username", "", "quay.io/ethcscs/private-example"]
        expected_message = "Invalid username: empty value provided"
        self._check(command, expected_message)

        command = ["bash", "-c", "printf 'invalid-password' | sarus pull --username invalid-username --login quay.io/ethcscs/private-example"]
        expected_message = ("Failed to pull image 'quay.io/ethcscs/private-example:latest'"
                            "\nUnable to retrieve auth token: invalid username or password provided.")
        self._check(command, expected_message)

        command = ["sarus", "pull", "--password-stdin", "--login", "quay.io/ethcscs/private-example"]
        expected_message = "The options '--password-stdin' and '--login' cannot be used together"
        self._check(command, expected_message)

        command = ["bash", "-c", "printf 'invalid-password' | sarus pull --username invalid-username --password-stdin quay.io/ethcscs/private-example"]
        expected_message = ("Failed to pull image 'quay.io/ethcscs/private-example:latest'"
                            "\nUnable to retrieve auth token: invalid username or password provided.")
        self._check(command, expected_message)

    def test_command_rmi(self):
        command = ["sarus", "rmi", "--invalid-option", self.DEFAULT_IMAGE]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi"]
        expected_message = "Too few arguments for command 'rmi'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", self.DEFAULT_IMAGE, "extra-argument"]
        expected_message = "Too many arguments for command 'rmi'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", self.DEFAULT_IMAGE, "--invalid-option"]
        expected_message = "Too many arguments for command 'rmi'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", "///"]
        expected_message = "Invalid image reference '///'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", "invalid-image-9as7302j"]
        expected_message = "Cannot find image 'docker.io/library/invalid-image-9as7302j:latest'"
        self._check(command, expected_message)

    def test_command_run(self):
        command = ["sarus", "run"]
        expected_message = "Too few arguments for command 'run'\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "///", "true"]
        expected_message = "Invalid image reference '///'\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "not-available-image", "true"]
        default_server_expected_message = ("Image docker.io/library/not-available-image:latest is not available. "
                                           "Attempting to look for equivalent image in index.docker.io server repositories")
        legacy_default_server_expected_message = "Image index.docker.io/library/not-available-image:latest is not available"
        self._check(command, default_server_expected_message)
        self._check(command, legacy_default_server_expected_message)

        # Error message when running by digest and only corresponding tagged image is available
        util.remove_image_if_necessary(is_centralized_repository=False,
                                       image="quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")
        util.pull_image_if_necessary(is_centralized_repository=False, image="quay.io/ethcscs/alpine:3.14")
        command = ["sarus", "run", "quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d", "true"]
        expected_message = "Image quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d is not available"
        self._check(command, expected_message)

        # Error message when running by tag+digest and only corresponding tagged image is available
        command = ["sarus", "run", "quay.io/ethcscs/alpine:3.14@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d", "true"]
        expected_message = "Image quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d is not available"
        self._check(command, expected_message)

        command = ["sarus", "run", "--invalid-option", self.DEFAULT_IMAGE, "true"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--workdir=/usr", "--workdir", "/tmp", self.DEFAULT_IMAGE, "true"]
        expected_message = "option '--workdir' cannot be specified more than once\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--workdir=invalid", self.DEFAULT_IMAGE, "true"]
        expected_message = ("The working directory 'invalid' is invalid, it needs to be an absolute path.\n"
                            "See 'sarus help run'")
        self._check(command, expected_message)

        command = ["sarus", "run", "--mount=xyz", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid mount request 'xyz': 'type' must be specified"
        self._check(command, expected_message)

        command = ["sarus", "run", "--mount=src=/invalid-s87dfs9,dst=/dst,type=bind", self.DEFAULT_IMAGE, "true"]
        expected_message = ("Failed to bind mount /invalid-s87dfs9 on container\'s /dst: "
                            "Failed to find real path for mount source: No such file or directory")
        self._check(command, expected_message)

        command = ["sarus", "run", "--mount=src=/src,dst=/dst,type=bind,bind-propagation=slave", self.DEFAULT_IMAGE, "true"]
        expected_message = "'bind-propagation' is not a valid bind mount option"
        self._check(command, expected_message)

        sarus_ssh_dir = os.getenv("HOME") + "/.oci-hooks/ssh"
        shutil.rmtree(sarus_ssh_dir, ignore_errors=True)  # remove ssh keys
        command = ["sarus", "run", "--ssh", self.DEFAULT_IMAGE, "true"]
        expected_message = "Failed to check the SSH keys. Hint: try to generate the SSH keys with 'sarus ssh-keygen'."
        self._check(command, expected_message)

        command = ["sarus", "run", "--ssh", "--pid=host", self.DEFAULT_IMAGE, "true"]
        expected_message = ("The use of '--ssh' is incompatible with '--pid=host'. "
                            "The SSH hook requires the use of a private PID namespace")
        self._check(command, expected_message)

        command = ["sarus", "run", "--pid=private", "--pid", "host", self.DEFAULT_IMAGE, "true"]
        expected_message = "option '--pid' cannot be specified more than once\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--pid", self.DEFAULT_IMAGE, "true"]
        expected_message = (f"Incorrect value provided for --pid option: '{self.DEFAULT_IMAGE}'. "
                            "Supported values: 'host', 'private'.\nSee 'sarus help run'")
        self._check(command, expected_message)

        command = ["sarus", "run", "--env==value", self.DEFAULT_IMAGE, "true"]
        expected_message = "Error parsing environment variable requested from CLI '=value': " \
                           "Failed to parse key-value pair '=value': key is empty"
        self._check(command, expected_message)

        command = ["sarus", "run", "--env", "", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid environment variable requested from CLI: empty option value"
        self._check(command, expected_message)

        command = ["sarus", "run", "--annotation", "=value", self.DEFAULT_IMAGE, "true"]
        expected_message = "Error parsing annotation from CLI '=value': " \
                           "Failed to parse key-value pair '=value': key is empty"
        self._check(command, expected_message)

        command = ["sarus", "run", "--annotation", "", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid annotation requested from CLI: empty option value"
        self._check(command, expected_message)

        command = ["sarus", "run", self.DEFAULT_IMAGE, "invalid-command-2lk32ldk2"]
        actual_message = util.get_sarus_error_output(command)
        self.assertTrue("level=error" in actual_message)
        self.assertTrue("invalid-command-2lk32ldk2" in actual_message)
        self.assertTrue("executable file not found in $PATH\"" in actual_message)

        command = ["sarus", "run", self.DEFAULT_IMAGE, "ls", "invalid-directory-2lk32ldk2"]
        expected_message = "ls: invalid-directory-2lk32ldk2: No such file or directory"
        self._check(command, expected_message)

    def test_command_run_device_option(self):
        command = ["sarus", "run", "--device", "", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request: no values provided"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=:rw", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request ':rw': detected empty host device path"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse::rw", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse::rw': detected empty container device path"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=dev/fuse", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request 'dev/fuse': host device path 'dev/fuse' must be absolute"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:dev/cont_fuse:rw", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:dev/cont_fuse:rw': container device path 'dev/cont_fuse' must be absolute"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/non-existing-device", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/non-existing-device': " \
                           "Failed to check if file \"/dev/non-existing-device\" is a device file. " \
                           "Stat failed: No such file or directory"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/home", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/home': Source path \"/home\" is not a device file"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:/dev/kvm:/dev/nvme0:rw", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:/dev/kvm:/dev/nvme0:rw': too many tokens provided. " \
                           "The format of the option value must be at most '<host device>:<container device>:<access>'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:': Input string for device access is empty. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:/dev/cont_fuse:", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:/dev/cont_fuse:': Input string for device access is empty. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:rwmm", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:rwmm': Input string for device access 'rwmm' is longer than 3 characters. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:rrw", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:rrw': Input string for device access 'rrw' has repeated characters. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:ra", self.DEFAULT_IMAGE, "true"]
        expected_message = "Invalid device request '/dev/fuse:ra': Input string for device access 'ra' contains an invalid character. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

    def test_command_sshkeygen(self):
        command = ["sarus", "ssh-keygen", "--invalid-option"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help ssh-keygen'"
        self._check(command, expected_message)

        command = ["sarus", "ssh-keygen", "extra-argument"]
        expected_message = "Too many arguments for command 'ssh-keygen'\nSee 'sarus help ssh-keygen'"
        self._check(command, expected_message)

    def test_command_version(self):
        command = ["sarus", "version", "--invalid-option"]
        expected_message = "Command 'version' doesn't support options\nSee 'sarus help version'"
        self._check(command, expected_message)

        command = ["sarus", "version", "extra-argument"]
        expected_message = "Too many arguments for command 'version'\nSee 'sarus help version'"
        self._check(command, expected_message)

    def _check(self, command, expected_message):
        util.assert_sarus_raises_error_containing_text(command, expected_message)
