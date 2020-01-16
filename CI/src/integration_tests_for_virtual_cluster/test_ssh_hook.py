# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import re
import os
import shutil
import subprocess

import common.util as util

class TestSshHook(unittest.TestCase):

    # Run tests in musl-only containers as well as glib-only containers, in order to make
    # sure that the custom OpenSSH injected into the container is standalone (as expected)
    # and doesn't have any dependency on musl or glibc. It happened in the past that the
    # custom sshd depended on the NSS dynamic libraries of glibc.
    image_with_musl = "alpine:3.8"
    image_with_glibc = "debian:stretch"

    def test_ssh_hook(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self.image_with_musl)
        util.pull_image_if_necessary(is_centralized_repository=False, image=self.image_with_glibc)

        self._check_ssh_keys_generation()

        # check SSH from node0 to node1
        hostname_node1 = self._get_hostname_of_node1(self.image_with_musl)
        hostname_node1_through_ssh = self._get_hostname_of_node1_though_ssh(self.image_with_musl, hostname_node1)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)
        hostname_node1_through_ssh = self._get_hostname_of_node1_though_ssh(self.image_with_glibc, hostname_node1)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)

        # check SSH goes into container (not host)
        prettyname = self._get_prettyname_of_node1_through_ssh(self.image_with_musl, hostname_node1)
        self.assertEqual(prettyname, "Alpine Linux v3.8")
        prettyname = self._get_prettyname_of_node1_through_ssh(self.image_with_glibc, hostname_node1)
        self.assertEqual(prettyname, "Debian GNU/Linux 9 (stretch)")

    def _check_ssh_keys_generation(self):
        ssh_dir = os.environ['HOME'] + "/.sarus/ssh"
        fingerprint_command = ["bash", "-c", "find " + ssh_dir + " -type f -exec sum {} \; |sum"]

        # check keys generation
        if os.path.exists(ssh_dir):
            shutil.rmtree(ssh_dir)
        util.generate_ssh_keys()
        self.assertTrue(os.path.exists(ssh_dir))
        fingerprint = subprocess.check_output(fingerprint_command).decode()

        # check keys generation (no overwrite)
        util.generate_ssh_keys()
        fingerprint_unchanged = subprocess.check_output(fingerprint_command).decode()
        self.assertEqual(fingerprint, fingerprint_unchanged)

        # check keys generation (overwrite)
        util.generate_ssh_keys(overwrite=True)
        fingerprint_changed = subprocess.check_output(fingerprint_command).decode()
        self.assertNotEqual(fingerprint, fingerprint_changed)

    def _get_hostname_of_node1(self, image):
        command = [
            "sh",
            "-c",
            "[ $SLURM_NODEID -eq 1 ] && echo $(hostname); exit 0"
        ]
        out = util.run_command_in_container_with_slurm(image=image,
                                                       command=command,
                                                       options_of_srun_command=["-N2"],
                                                       options_of_run_command=["--ssh"])
        assert len(out) == 1 # expect one single line of output
        return out[0]

    def _get_hostname_of_node1_though_ssh(self, image, hostname_node1):
        command = [
            "sh",
            "-c",
            "if [ $SLURM_NODEID -eq 0 ]; then"
            "   echo $(ssh {} hostname);"
            "   ssh {} touch /done;"
            "else"
            "   while [ ! -e /done ]; do"
            "      sleep 1;"
            "   done;"
            "fi".format(hostname_node1, hostname_node1)
        ]
        out = util.run_command_in_container_with_slurm(image=image,
                                                       command=command,
                                                       options_of_srun_command=["-N2"],
                                                       options_of_run_command=["--ssh"])
        assert len(out) == 1 # expect one single line of output
        return out[0]

    def _get_prettyname_of_node1_through_ssh(self, image, hostname_node1):
        command = [
            "sh",
            "-c",
            "if [ $SLURM_NODEID -eq 0 ]; then"
            "   echo $(ssh {} cat /etc/os-release);"
            "   ssh {} touch /done;"
            "else"
            "   while [ ! -e /done ]; do"
            "      sleep 1;"
            "   done;"
            "fi".format(hostname_node1, hostname_node1)
        ]
        out = util.run_command_in_container_with_slurm(image=image,
                                                         command=command,
                                                         options_of_srun_command=["-N2"],
                                                         options_of_run_command=["--ssh"])
        assert len(out) == 1 # expect one single line of output
        return re.match(r".*PRETTY_NAME=\"([^\"]+)\" .*", out[0]).group(1)
