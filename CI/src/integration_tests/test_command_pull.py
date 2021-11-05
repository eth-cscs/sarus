# Sarus
#
# Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import json
import os
import subprocess
import unittest

import common.util as util


class TestCommandPull(unittest.TestCase):
    """
    These tests verify that the pulled images are available to the user,
    i.e. correctly listed through the "images" command.
    """

    @classmethod
    def setUpClass(cls):
        cls._backup_sarus_json()

    def tearDown(self):
        # Ensure each test has a fresh sarus json
        self._restore_sarus_json()

    @classmethod
    def tearDownClass(cls):
        # Will generally be redundant due to tearDown, included for completeness.
        cls._restore_sarus_json()

    @classmethod
    def _backup_sarus_json(cls):
        cls._sarusjson_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename, cls._sarusjson_filename+".bak"])

    @classmethod
    def _restore_sarus_json(cls):
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename+".bak", cls._sarusjson_filename])


    def test_command_pull_with_local_repository(self):
        self._test_command_pull("quay.io/ethcscs/alpine:3.14", is_centralized_repository=False)

    def test_command_pull_with_centralized_repository(self):
        self._test_command_pull("quay.io/ethcscs/alpine:3.14", is_centralized_repository=True)

    def test_command_pull_with_nvcr(self):
        self._test_command_pull("nvcr.io/nvidia/k8s-device-plugin:1.0.0-beta4",
                                is_centralized_repository=False)

    def test_command_pull_with_dockerhub(self):
        self._test_command_pull("ubuntu:20.04",
                                is_centralized_repository=False)

    def test_command_pull_fails_with_insecure_registry(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self._test_command_pull("localhost:5000/library/alpine:latest",
                                    is_centralized_repository=False)

    def test_command_pull_succeeds_with_insecure_registry_when_configured(self):
        self._add_insecure_registries_entry("localhost:5000")

        self._test_command_pull("localhost:5000/library/alpine:latest",
                                is_centralized_repository=False)

    def test_command_pull_fails_with_insecure_registry_not_in_config(self):
        self._add_insecure_registries_entry("some.other.insecure.registry")

        with self.assertRaises(subprocess.CalledProcessError):
            self._test_command_pull("localhost:5000/library/alpine:latest",
                                    is_centralized_repository=False)

    def test_multiple_pulls_with_local_repository(self):
        self._test_multiple_pulls("quay.io/ethcscs/alpine:3.14", is_centralized_repository=False)

    def test_multiple_pulls_with_centralized_repository(self):
        self._test_multiple_pulls("quay.io/ethcscs/alpine:3.14", is_centralized_repository=True)

    def _test_command_pull(self, image, is_centralized_repository):
        util.remove_image_if_necessary(is_centralized_repository, image)
        actual_images = util.list_images(is_centralized_repository)
        self.assertEqual(actual_images.count(image), 0)

        util.pull_image_if_necessary(is_centralized_repository, image)
        actual_images = util.list_images(is_centralized_repository)
        self.assertEqual(actual_images.count(image), 1)

    # check that multiple pulls of the same image don't generate
    # multiple entries in the list of available images
    def _test_multiple_pulls(self, image, is_centralized_repository):
        for i in range(2):
            util.pull_image(is_centralized_repository, image)
            actual_images = util.list_images(is_centralized_repository)
            self.assertEqual(actual_images.count(image), 1)

    def _add_insecure_registries_entry(self, registry_address):
        with open(self._sarusjson_filename, 'r') as f:
            config = json.load(f)

        config["insecureRegistries"] = [ registry_address ]

        with open("sarus.json.dummy", "w") as f:
            json.dump(config, f)

        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", self._sarusjson_filename])
        os.remove("sarus.json.dummy")
