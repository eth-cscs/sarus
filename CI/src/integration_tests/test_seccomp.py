# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os

import common.util as util


class TestSeccomp(unittest.TestCase):
    """
    These tests verify that using the seccompProfile parameter in sarus.json
    correctly applies the specified profile to containers
    """

    @classmethod
    def setUpClass(cls):
        cls._create_seccomp_profile()
        util.modify_sarus_json({"seccompProfile": cls.seccomp_test_file})
        cls._pull_docker_images()

    @classmethod
    def tearDownClass(cls):
        cls._remove_seccomp_profile()
        util.restore_sarus_json()

    @classmethod
    def _pull_docker_images(cls):
        images = [util.ALPINE_IMAGE,
                  util.UBUNTU_IMAGE,
                  "quay.io/ethcscs/fedora:34"]
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
            import json
            json.dump(profile, f)

    @classmethod
    def _remove_seccomp_profile(cls):
        os.remove(cls.seccomp_test_file)

    def test_deny_chdir(self):
        sarus_options = []
        container_args = ["bash", "-c", "cd /tmp"]
        self._assert_sarus_raises_error_containing_text(sarus_options,
                                                        util.UBUNTU_IMAGE,
                                                        container_args,
                                                        "/tmp: Operation not permitted")
        self._assert_sarus_raises_error_containing_text(sarus_options,
                                                        "quay.io/ethcscs/fedora:34",
                                                        container_args,
                                                        "/tmp: Operation not permitted")
        container_args = ["sh", "-c", "cd /tmp"]
        self._assert_sarus_raises_error_containing_text(sarus_options,
                                                        util.ALPINE_IMAGE,
                                                        container_args,
                                                        "/tmp: Operation not permitted")

    def _assert_sarus_raises_error_containing_text(self, sarus_options, container_image, container_args, text):
        command = ["sarus", "run"] + sarus_options + [container_image] + container_args
        util.assert_sarus_raises_error_containing_text(command, text)
