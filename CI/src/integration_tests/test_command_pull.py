# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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

    def test_command_pull_by_digest(self):
        self._test_command_pull("quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                is_centralized_repository=False,
                                expected_string="quay.io/ethcscs/alpine:<none>@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

    def test_command_pull_by_tag_and_digest(self):
        self._test_command_pull("quay.io/ethcscs/alpine:3.14@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                is_centralized_repository=False,
                                expected_string="quay.io/ethcscs/alpine:<none>@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d")

    def test_command_pull_with_uptodate_image_in_local_repository(self):
        self._test_command_pull_with_uptodate_image("quay.io/ethcscs/ubuntu:20.04", False)

    def test_command_pull_with_uptodate_image_in_centralized_repository(self):
        self._test_command_pull_with_uptodate_image("quay.io/ethcscs/ubuntu:20.04", True)

    def test_command_pull_with_uptodate_image_by_digest(self):
        self._test_command_pull_with_uptodate_image("quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                                    False)

    def test_command_pull_with_uptodate_image_by_tag_and_digest(self):
        self._test_command_pull_with_uptodate_image("quay.io/ethcscs/alpine:3.14@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d",
                                                    False)

    @unittest.skip("Not implemented yet")
    def test_command_pull_fails_with_insecure_registry(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self._test_command_pull("localhost:5000/library/alpine:latest",
                                    is_centralized_repository=False)

    @unittest.skip("Not implemented yet")
    def test_command_pull_succeeds_with_insecure_registry_when_configured(self):
        self._add_insecure_registries_entry("localhost:5000")

        self._test_command_pull("localhost:5000/library/alpine:latest",
                                is_centralized_repository=False)

    @unittest.skip("Not implemented yet")
    def test_command_pull_fails_with_insecure_registry_not_in_config(self):
        self._add_insecure_registries_entry("some.other.insecure.registry")

        with self.assertRaises(subprocess.CalledProcessError):
            self._test_command_pull("localhost:5000/library/alpine:latest",
                                    is_centralized_repository=False)

    def test_multiple_pulls_with_local_repository(self):
        self._test_multiple_pulls("quay.io/ethcscs/alpine:3.14", is_centralized_repository=False)

    def test_multiple_pulls_with_centralized_repository(self):
        self._test_multiple_pulls("quay.io/ethcscs/alpine:3.14", is_centralized_repository=True)

    def test_pull_after_missing_image_file(self):
        import pwd, pathlib
        image = "quay.io/ethcscs/alpine:3.14"
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image=image)
        with open(self.__class__._sarusjson_filename) as sarus_json:
            config = json.load(sarus_json)
            repo_base = config["localRepositoryBaseDir"]

        username = pwd.getpwuid(os.geteuid()).pw_name
        image_backing_file = pathlib.Path(repo_base, username, ".sarus/images/quay.io/ethcscs/alpine/3.14.squashfs")
        image_backing_file.unlink()

        error_message = util.get_sarus_error_output(["sarus", "run", image, "true"])
        assert "Storage inconsistency detected" in error_message

        util.pull_image(is_centralized_repository=False,
                        image="quay.io/ethcscs/alpine:3.14")
        assert util.run_image_and_get_prettyname(is_centralized_repository=False,
                                                 image=image).startswith("Alpine Linux")

    def _test_command_pull(self, image, is_centralized_repository, expected_string=None):
        expected_image = expected_string if expected_string else image

        util.remove_image_if_necessary(is_centralized_repository, image)
        self.assertFalse(util.is_image_available(is_centralized_repository, expected_image))

        util.pull_image_if_necessary(is_centralized_repository, image)
        self.assertTrue(util.is_image_available(is_centralized_repository, expected_image))

    def _test_command_pull_with_uptodate_image(self, image, is_centralized_repository):
        util.pull_image_if_necessary(is_centralized_repository, image)
        output = util.pull_image(is_centralized_repository, image)
        self.assertEqual(output[-1], f"Image for {image} is already available and up to date")

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
