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

    image = "library/debian:stretch"

    def test_ssh_hook(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self.image)

        self._check_ssh_keys_generation()

        # check SSH from node0 to node1
        hostname_node1 = self._get_hostname_of_node1()
        hostname_node1_through_ssh = self._get_hostname_of_node1_though_ssh(hostname_node1)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)

        # check SSH goes into container (not host)
        prettyname = self._get_prettyname_of_node1_through_ssh(hostname_node1)
        self.assertEqual(prettyname, "Debian GNU/Linux 9 (stretch)")

    def _check_ssh_keys_generation(self):
        ssh_dir = os.environ['HOME'] + "/.sarus/ssh"
        fingerprint_command = ["bash", "-c", "find " + ssh_dir + " -type f -exec sum \{\} \; |sum"]

        # check keys generation
        shutil.rmtree(ssh_dir)
        util.generate_ssh_keys()
        self.assertTrue(os.path.exists(ssh_dir))
        fingerprint = subprocess.check_output(fingerprint_command)

        # check keys generation (no overwrite)
        util.generate_ssh_keys()
        fingerprint_unchanged = subprocess.check_output(fingerprint_command)
        self.assertEqual(fingerprint, fingerprint_unchanged)

        # check keys generation (overwrite)
        util.generate_ssh_keys(overwrite=True)
        fingerprint_changed = subprocess.check_output(fingerprint_command)
        self.assertNotEqual(fingerprint, fingerprint_changed)

    def _get_hostname_of_node1(self):
        command = [
            "bash",
            "-c",
            "[ $SLURM_NODEID -eq 1 ] && echo $(hostname); exit 0"
        ]
        out = util.run_command_in_container_with_slurm(image=self.image,
                                                       command=command,
                                                       options_of_srun_command=["-N2"],
                                                       options_of_run_command=["--ssh"])
        assert len(out) == 1 # expect one single line of output
        return out[0]

    def _get_hostname_of_node1_though_ssh(self, hostname_node1):
        command = [
            "bash",
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
        out = util.run_command_in_container_with_slurm(image=self.image,
                                                       command=command,
                                                       options_of_srun_command=["-N2"],
                                                       options_of_run_command=["--ssh"])
        assert len(out) == 1 # expect one single line of output
        return out[0]

    def _get_prettyname_of_node1_through_ssh(self, hostname_node1):
        command = [
            "bash",
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
        out = util.run_command_in_container_with_slurm(image=self.image,
                                                         command=command,
                                                         options_of_srun_command=["-N2"],
                                                         options_of_run_command=["--ssh"])
        assert len(out) == 1 # expect one single line of output
        return re.match(r".*PRETTY_NAME=\"([^\"]+)\" .*", out[0]).group(1)
