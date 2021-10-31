# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import shutil
import subprocess

import common.util as util


class TestErrorMessages(unittest.TestCase):
    """
    These tests verify that Sarus generates user-friendly error messages
    when the user's input is invalid.
    """

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image="quay.io/ethcscs/alpine")

    def test_sarus(self):
        command = ["sarus", "--invalid-option"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help'"
        self._check(command, expected_message)

        command = ["sarus", "invalid-command"]
        expected_message = "'invalid-command' is not a Sarus command\nSee 'sarus help'"
        self._check(command, expected_message)

        command = ["sarus", "--verbose", "---run"]
        actual_message = self._get_sarus_error_output(command)
        self.assertTrue("'---run' is not a Sarus command" in actual_message)

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
        command = ["sarus", "load", "--invalid-option", "archive.tar", "imageid"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load"]
        expected_message = "Too few arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar"]
        expected_message = "Too few arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "imageid", "extra-argument"]
        expected_message = "Too many arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "alpine", "--invalid-option"]
        expected_message = "Too many arguments for command 'load'\nSee 'sarus help load'"
        self._check(command, expected_message)

        command = ["sarus", "load", "archive.tar", "///"]
        expected_message = ("Invalid image ID '///'\nSee 'sarus help load'")
        self._check(command, expected_message)

        command = ["sarus", "load", "--temp-dir=/invalid-dir", "archive.tar", "imageid"]
        expected_message = "Invalid temporary directory \"/invalid-dir\""
        self._check(command, expected_message)

    def test_command_pull(self):
        command = ["sarus", "pull", "--invalid-option", "quay.io/ethcscs/alpine"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull"]
        expected_message = "Too few arguments for command 'pull'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", "quay.io/ethcscs/alpine", "extra-argument"]
        expected_message = "Too many arguments for command 'pull'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", "quay.io/ethcscs/alpine", "--invalid-option"]
        expected_message = "Too many arguments for command 'pull'\nSee 'sarus help pull'"
        self._check(command, expected_message)

        command = ["sarus", "pull", "///"]
        expected_message = ("Invalid image ID '///'\nSee 'sarus help pull'")
        self._check(command, expected_message)

        command = ["sarus", "pull", "invalid-image-1kds710dkj"]
        expected_message = ("Failed to pull image 'index.docker.io/library/invalid-image-1kds710dkj:latest'"
                            "\nThe image may be private or not present in the remote registry."
                            "\nDid you perform a login with the proper credentials?"
                            "\nSee 'sarus help pull' (--login option)")
        self._check(command, expected_message)

        command = ["sarus", "pull", "quay.io/ethcscs/private-example"]
        expected_message = ("Failed to pull image 'quay.io/ethcscs/private-example:latest'"
                            "\nThe image may be private or not present in the remote registry."
                            "\nDid you perform a login with the proper credentials?"
                            "\nSee 'sarus help pull' (--login option)")
        self._check(command, expected_message)

        command = ["bash", "-c", "printf 'invalid-username\ninvalid-password' |sarus pull --login quay.io/ethcscs/private-example"]
        expected_message = ("Authorization failed when retrieving token for image 'quay.io/ethcscs/private-example:latest'"
                            "\nPlease check the entered credentials.")
        self._check(command, expected_message)

        command = ["sarus", "pull", "--temp-dir=/invalid-dir", "quay.io/ethcscs/alpine"]
        expected_message = "Invalid temporary directory \"/invalid-dir\""
        self._check(command, expected_message)

    def test_command_rmi(self):
        command = ["sarus", "rmi", "--invalid-option", "quay.io/ethcscs/alpine"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi"]
        expected_message = "Too few arguments for command 'rmi'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", "quay.io/ethcscs/alpine", "extra-argument"]
        expected_message = "Too many arguments for command 'rmi'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", "quay.io/ethcscs/alpine", "--invalid-option"]
        expected_message = "Too many arguments for command 'rmi'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", "///"]
        expected_message = "Invalid image ID '///'\nSee 'sarus help rmi'"
        self._check(command, expected_message)

        command = ["sarus", "rmi", "invalid-image-9as7302j"]
        expected_message = "Cannot find image 'index.docker.io/library/invalid-image-9as7302j:latest'"
        self._check(command, expected_message)

    def test_command_run(self):
        command = ["sarus", "run"]
        expected_message = "Too few arguments for command 'run'\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "///", "true"]
        expected_message = "Invalid image ID '///'\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "not-available-image", "true"]
        expected_message = "Specified image index.docker.io/library/not-available-image:latest is not available"
        self._check(command, expected_message)

        command = ["sarus", "run", "--invalid-option", "quay.io/ethcscs/alpine", "true"]
        expected_message = "unrecognised option '--invalid-option'\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--workdir=/usr", "--workdir", "/tmp", "quay.io/ethcscs/alpine", "true"]
        expected_message = "option '--workdir' cannot be specified more than once\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--workdir=invalid", "quay.io/ethcscs/alpine", "true"]
        expected_message = ("The working directory 'invalid' is invalid, it needs to be an absolute path.\n"
                            "See 'sarus help run'")
        self._check(command, expected_message)

        command = ["sarus", "run", "--mount=xyz", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid mount request 'xyz': 'type' must be specified"
        self._check(command, expected_message)

        command = ["sarus", "run", "--mount=src=/invalid-s87dfs9,dst=/dst,type=bind", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Failed to bind mount /invalid-s87dfs9 on container\'s /dst: mount source doesn\'t exist"
        self._check(command, expected_message)

        command = ["sarus", "run", "--mount=src=/src,dst=/dst,type=bind,bind-propagation=slave", "quay.io/ethcscs/alpine", "true"]
        expected_message = "'bind-propagation' is not a valid bind mount option"

        sarus_ssh_dir = os.getenv("HOME") + "/.oci-hooks/ssh"
        shutil.rmtree(sarus_ssh_dir, ignore_errors=True) # remove ssh keys
        command = ["sarus", "run", "--ssh", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Failed to check the SSH keys. Hint: try to generate the SSH keys with 'sarus ssh-keygen'."
        self._check(command, expected_message)

        command = ["sarus", "run", "--ssh", "--pid=host", "quay.io/ethcscs/alpine", "true"]
        expected_message = ("The use of '--ssh' is incompatible with '--pid=host'. "
                            "The SSH hook requires the use of a private PID namespace")
        self._check(command, expected_message)

        command = ["sarus", "run", "--pid=private", "--pid", "host", "quay.io/ethcscs/alpine", "true"]
        expected_message = "option '--pid' cannot be specified more than once\nSee 'sarus help run'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--pid", "quay.io/ethcscs/alpine", "true"]
        expected_message = ("Incorrect value provided for --pid option: 'quay.io/ethcscs/alpine'. "
                            "Supported values: 'host', 'private'.\nSee 'sarus help run'")
        self._check(command, expected_message)

        command = ["sarus", "run", "quay.io/ethcscs/alpine", "invalid-command-2lk32ldk2"]
        actual_message = self._get_sarus_error_output(command)
        self.assertTrue("container_linux.go" in actual_message)
        self.assertTrue("starting container process caused" in actual_message)
        self.assertTrue("invalid-command-2lk32ldk2" in actual_message)
        self.assertTrue("executable file not found in $PATH\"" in actual_message)

        command = ["sarus", "run", "quay.io/ethcscs/alpine", "ls", "invalid-directory-2lk32ldk2"]
        expected_message = "ls: invalid-directory-2lk32ldk2: No such file or directory"
        self._check(command, expected_message)

    def test_command_run_device_option(self):
        command = ["sarus", "run", "--device", "", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request: no values provided"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=:rw", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request ':rw': detected empty host device path"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse::rw", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse::rw': detected empty container device path"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=dev/fuse", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request 'dev/fuse': host device path 'dev/fuse' must be absolute"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:dev/cont_fuse:rw", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse:dev/cont_fuse:rw': container device path 'dev/cont_fuse' must be absolute"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/non-existing-device", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/non-existing-device': " \
                           "Failed to check if file \"/dev/non-existing-device\" is a device file. " \
                           "Stat failed: No such file or directory"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/home", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/home': Source path \"/home\" is not a device file"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:/dev/kvm:/dev/nvme0:rw", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse:/dev/kvm:/dev/nvme0:rw': too many tokens provided. " \
                           "The format of the option value must be at most '<host device>:<container device>:<access>'"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse:': Input string for device access is empty. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:/dev/cont_fuse:", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse:/dev/cont_fuse:': Input string for device access is empty. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:rwmm", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse:rwmm': Input string for device access 'rwmm' is longer than 3 characters. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:rrw", "quay.io/ethcscs/alpine", "true"]
        expected_message = "Invalid device request '/dev/fuse:rrw': Input string for device access 'rrw' has repeated characters. " \
                           "Device access must be entered as a combination of 'rwm' characters, with no repetitions"
        self._check(command, expected_message)

        command = ["sarus", "run", "--device=/dev/fuse:ra", "quay.io/ethcscs/alpine", "true"]
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
        actual_message = self._get_sarus_error_output(command)
        assert expected_message in actual_message

    def _get_sarus_error_output(self, command):
        with open(os.devnull, 'wb') as devnull:
            proc = subprocess.run(command, stdout=devnull, stderr=subprocess.PIPE)
            if proc.returncode == 0:
                self.fail("Sarus didn't generate any error, but at least one was expected.")

            stderr_without_trailing_whitespaces = proc.stderr.rstrip()
            out = stderr_without_trailing_whitespaces.decode()

        # filter out profiling messages
        lines_filtered = []
        for line in out.split('\n'):
            if not line.startswith("profiling:"):
                lines_filtered.append(line)

        return '\n'.join(lines_filtered)

if __name__ == "__main__":
    unittest.main()
