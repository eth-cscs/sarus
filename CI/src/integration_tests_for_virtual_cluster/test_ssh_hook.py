# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
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

    # Run tests in musl-only containers as well as glibc-only containers, in order to make
    # sure that the custom OpenSSH injected into the container is standalone (as expected)
    # and doesn't have any dependency on musl or glibc. It happened in the past that the
    # custom sshd depended on the NSS dynamic libraries of glibc.
    image_with_musl = "quay.io/ethcscs/alpine:3.14"
    image_with_glibc = "quay.io/ethcscs/debian:buster"

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

        # check SSH from node0 to node1 with non-standard $HOME
        # in some systems the home from the passwd file (copied from the host)
        # might not be present in the container. The hook has to deduce it from
        # the passwd file and create it.
        self._set_home_in_sarus_passwd_file("/users/test")
        hostname_node1_through_ssh = self._get_hostname_of_node1_though_ssh(self.image_with_musl, hostname_node1)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)
        hostname_node1_through_ssh = self._get_hostname_of_node1_though_ssh(self.image_with_glibc, hostname_node1)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)
        self._restore_sarus_passwd_file()

        # check SSH goes into container (not host)
        prettyname = self._get_prettyname_of_node1_through_ssh(self.image_with_musl, hostname_node1)
        self.assertEqual(prettyname, "Alpine Linux v3.14")
        prettyname = self._get_prettyname_of_node1_through_ssh(self.image_with_glibc, hostname_node1)
        self.assertEqual(prettyname, "Debian GNU/Linux 10 (buster)")

    def _check_ssh_keys_generation(self):
        ssh_dir = os.environ['HOME'] + "/.oci-hooks/ssh"
        fingerprint_command = ["bash", "-c", "find " + ssh_dir + r" -type f -exec sum {} \; | sum"]

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

    def _set_home_in_sarus_passwd_file(self, new_home_directory):
        import getpass
        self.saruspwd_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/passwd"

        # Create backup
        subprocess.check_output(["sudo", "cp", self.saruspwd_filename,
                                 self.saruspwd_filename + '.bak'])

        with open('/etc/passwd', 'r') as f:
            pwdlines = f.readlines()

        userline = [l for l in pwdlines if l.startswith(getpass.getuser() + ':')][0]
        user_fields = userline.split(':')
        user_fields[5] = new_home_directory
        user_index = pwdlines.index(userline)
        pwdlines[user_index] = ':'.join(user_fields)

        with open("passwd.dummy", 'w') as f:
            f.writelines(pwdlines)

        subprocess.check_output(["sudo", "cp", "passwd.dummy", self.saruspwd_filename])
        os.remove("passwd.dummy")

    def _restore_sarus_passwd_file(self):
        subprocess.check_output(["sudo", "cp", self.saruspwd_filename + '.bak', self.saruspwd_filename])
