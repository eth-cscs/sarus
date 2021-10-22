# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import shutil
import json

import common.util as util


class TestSeccomp(unittest.TestCase):
    """
    These tests verify that using the seccompProfile parameter in sarus.json
    correctly applies the specified profile to containers
    """

    @classmethod
    def setUpClass(cls):
        cls._create_seccomp_profile()
        cls._modify_sarusjson_file()
        cls._pull_docker_images()

    @classmethod
    def tearDownClass(cls):
        cls._remove_seccomp_profile()
        cls._restore_sarusjson_file()

    @classmethod
    def _pull_docker_images(cls):
        images = ["quay.io/ethcscs/ubuntu:20.04",
                  "quay.io/ethcscs/fedora:34",
                  "quay.io/ethcscs/alpine:3.14"]
        for image in images:
            util.pull_image_if_necessary(is_centralized_repository=False, image=image)

    @classmethod
    def _create_seccomp_profile(cls):
        profile = {
            "defaultAction": "SCMP_ACT_ALLOW",
            "architectures": [
                "SCMP_ARCH_X86",
                "SCMP_ARCH_X32"
            ],
            "syscalls": [
                {
                    "names": [
                        "chdir"
                    ],
                    "action": "SCMP_ACT_ERRNO"
                }
            ]
        }
        cls.seccomp_test_file = os.path.join(os.getcwd(), "sarus_seccomp_test.json")
        with open(cls.seccomp_test_file, 'w') as f:
            json.dump(profile, f)

    @classmethod
    def _remove_seccomp_profile(cls):
        os.remove(cls.seccomp_test_file)

    @classmethod
    def _modify_sarusjson_file(cls):
        cls._sarusjson_filename = os.environ[
                                      "CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"
        # Create backup
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename,
                                 cls._sarusjson_filename + '.bak'])
        # Modify config file
        with open(cls._sarusjson_filename, 'r') as f:
            config = json.load(f)
        config["seccompProfile"] = cls.seccomp_test_file
        with open("sarus.json.dummy", 'w') as f:
            json.dump(config,f)
        subprocess.check_output(
            ["sudo", "cp", "sarus.json.dummy", cls._sarusjson_filename])
        os.remove("sarus.json.dummy")

    @classmethod
    def _restore_sarusjson_file(cls):
        subprocess.check_output(
            ["sudo", "cp", cls._sarusjson_filename + '.bak', cls._sarusjson_filename])

    def test_deny_chdir(self):
        sarus_options = []
        container_args = ["bash", "-c", "cd /tmp"]
        self._assert_sarus_raises_error_containing_text(sarus_options,
                                                        "quay.io/ethcscs/ubuntu:20.04",
                                                        container_args,
                                                        "/tmp: Operation not permitted")
        self._assert_sarus_raises_error_containing_text(sarus_options,
                                                        "quay.io/ethcscs/fedora:34",
                                                        container_args,
                                                        "/tmp: Operation not permitted")
        container_args = ["sh", "-c", "cd /tmp"]
        self._assert_sarus_raises_error_containing_text(sarus_options,
                                                        "quay.io/ethcscs/alpine:3.14",
                                                        container_args,
                                                        "/tmp: Operation not permitted")

    def _assert_sarus_raises_error_containing_text(self, sarus_options, container_image, container_args, text):
        sarus_output = self._get_sarus_error_output(sarus_options, container_image, container_args)
        assert text in sarus_output, 'Sarus generated an error, but it did not contain the expected text "{}".'.format(text)

    def _get_sarus_error_output(self, sarus_options, container_image, container_args):
        command = ["sarus", "run"] + sarus_options + [container_image] + container_args
        try:
            subprocess.check_output(command, stderr=subprocess.STDOUT)
            raise Exception(
                "Sarus didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            return ex.output.decode()
