# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import re
import os
import shutil
import subprocess
from contextlib import contextmanager

import common.util as util


class TestSshHook(unittest.TestCase):

    # Run tests in musl-only containers as well as glibc-only containers, in order to make
    # sure that the SSH software injected into the container is standalone (as expected)
    # and doesn't have any dependency on musl or glibc. It happened in the past that the
    # custom sshd depended on the NSS dynamic libraries of glibc.
    image_with_musl = "quay.io/ethcscs/alpine:3.14"
    image_with_glibc = "quay.io/ethcscs/debian:buster"

    SARUS_INSTALL_PATH = os.environ["CMAKE_INSTALL_PREFIX"]

    OCI_HOOKS_CONFIG_FILES = {
        "ssh": SARUS_INSTALL_PATH + "/etc/hooks.d/ssh_hook.json",
        "slurm_sync": SARUS_INSTALL_PATH + "/etc/hooks.d/slurm_sync_hook.json"
    }

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._get_hooks_base_dir()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.image_with_musl)
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.image_with_glibc)

    @classmethod
    def _get_hooks_base_dir(cls):
        import json
        with open(util.sarus_json_filename, 'r') as f:
            config = json.load(f)
        cls.OCI_HOOKS_BASE_DIR = config["localRepositoryBaseDir"]

    @classmethod
    def _generate_ssh_hook_config(cls):
        hook_config = {
            "version": "1.0.0",
            "hook": {
                "path": f"{cls.SARUS_INSTALL_PATH}/bin/ssh_hook",
                "env": [
                    f"HOOK_BASE_DIR={cls.OCI_HOOKS_BASE_DIR}",
                    f"PASSWD_FILE={cls.SARUS_INSTALL_PATH}/etc/passwd",
                    f"DROPBEAR_DIR={cls.SARUS_INSTALL_PATH}/dropbear",
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
            "stages": ["prestart"]
        }
        return hook_config

    @classmethod
    def _generate_slurm_sync_hook_config(cls):
        hook_config = {
            "version": "1.0.0",
            "hook": {
                "path": f"{cls.SARUS_INSTALL_PATH}/bin/slurm_global_sync_hook",
                "env": [
                    f"HOOK_BASE_DIR={cls.OCI_HOOKS_BASE_DIR}",
                    f"PASSWD_FILE={cls.SARUS_INSTALL_PATH}/etc/passwd"
                ]
            },
            "when": {
                "annotations": {
                    "^com.hooks.slurm-global-sync.enabled$": "^true$"
                }
            },
            "stages": ["prestart"]
        }
        return hook_config

    def test_ssh_hook(self):
        with util.temporary_hook_files((self._generate_ssh_hook_config(), self.OCI_HOOKS_CONFIG_FILES["ssh"]),
                                       (self._generate_slurm_sync_hook_config(), self.OCI_HOOKS_CONFIG_FILES["slurm_sync"])):
            util.generate_ssh_keys()

            # check SSH from node0 to node1
            hostname_node1 = get_hostname_of_node1(self.image_with_musl)
            self._check_ssh_hook(hostname_node1)

            # check SSH from node0 to node1 with non-standard $HOME
            # in some systems the home from the passwd file (copied from the host)
            # might not be present in the container. The hook has to deduce it from
            # the passwd file and create it.
            with custom_home_in_sarus_passwd_file("/users/test"):
                self._check_ssh_hook(hostname_node1)

            # check SSH from node0 to node1 with server port set from Sarus CLI
            self._check_ssh_hook(hostname_node1, ["--annotation=com.hooks.ssh.port=33333"])


    def test_ssh_keys_generation(self):
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

    def _check_ssh_hook(self, hostname_node1, sarus_run_extra_opts=[]):
        hostname_node1_through_ssh = get_hostname_of_node1_though_ssh(self.image_with_musl,
                                                                      hostname_node1,
                                                                      sarus_run_extra_opts)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)
        hostname_node1_through_ssh = get_hostname_of_node1_though_ssh(self.image_with_glibc,
                                                                      hostname_node1,
                                                                      sarus_run_extra_opts)
        self.assertEqual(hostname_node1, hostname_node1_through_ssh)

        # check SSH goes into container (not host)
        prettyname = get_prettyname_of_node1_through_ssh(self.image_with_musl,
                                                         hostname_node1,
                                                         sarus_run_extra_opts)
        self.assertEqual(prettyname, "Alpine Linux v3.14")
        prettyname = get_prettyname_of_node1_through_ssh(self.image_with_glibc,
                                                         hostname_node1,
                                                         sarus_run_extra_opts)
        self.assertEqual(prettyname, "Debian GNU/Linux 10 (buster)")


def get_hostname_of_node1(image):
    command = [
        "sh",
        "-c",
        "[ $SLURM_NODEID -eq 1 ] && echo $(hostname); exit 0"
    ]
    out = util.run_command_in_container_with_slurm(image=image,
                                                   command=command,
                                                   options_of_srun_command=["-N2"],
                                                   options_of_run_command=["--ssh"])
    assert len(out) == 1  # expect one single line of output
    return out[0]


def get_hostname_of_node1_though_ssh(image, hostname_node1, sarus_run_extra_opts=[]):
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
                                                   options_of_run_command=["--ssh"]+sarus_run_extra_opts)
    assert len(out) == 1  # expect one single line of output
    return out[0]


def get_prettyname_of_node1_through_ssh(image, hostname_node1, sarus_run_extra_opts=[]):
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
                                                   options_of_run_command=["--ssh"]+sarus_run_extra_opts)
    assert len(out) == 1  # expect one single line of output
    return re.match(r".*PRETTY_NAME=\"([^\"]+)\" .*", out[0]).group(1)


@contextmanager
def custom_home_in_sarus_passwd_file(new_home_directory):
    try:
        import getpass
        saruspwd_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/passwd"

        # Create backup
        subprocess.check_output(["sudo", "cp", saruspwd_filename,
                                 saruspwd_filename + '.bak'])

        with open('/etc/passwd', 'r') as f:
            pwdlines = f.readlines()

        userline = [l for l in pwdlines if l.startswith(getpass.getuser() + ':')][0]
        user_fields = userline.split(':')
        user_fields[5] = new_home_directory
        user_index = pwdlines.index(userline)
        pwdlines[user_index] = ':'.join(user_fields)

        with open("passwd.dummy", 'w') as f:
            f.writelines(pwdlines)

        subprocess.check_output(["sudo", "cp", "passwd.dummy", saruspwd_filename])
        os.remove("passwd.dummy")
        yield

    finally:
        subprocess.check_output(["sudo", "cp", saruspwd_filename + '.bak', saruspwd_filename])
