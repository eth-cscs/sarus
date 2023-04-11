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


class TestUserMounts(unittest.TestCase):
    """
    These tests verify that using the --mount CLI option correctly bind-mounts
    the requested directories into the container.
    """

    CONTAINER_IMAGE = util.UBUNTU_IMAGE
    SOURCE_DIR = None
    TEST_FILES = ["a.txt", "b.md", "c.py", "subdir/d.cpp", "subdir/e.pdf"]
    CHECK_TEMPLATE = ("for fname in {files}; do"
                      "    if [ ! -f $fname ]; then"
                      "         echo \"FAIL\"; "
                      "    fi; "
                      "done; "
                      "echo \"PASS\" ")

    @classmethod
    def setUpClass(cls):
        cls._create_source_directory()
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)

    @classmethod
    def tearDownClass(cls):
        cls._remove_source_directory()

    @classmethod
    def _create_source_directory(cls):
        cls.SOURCE_DIR = os.path.join(os.getcwd(), "sarus-test", "mount_source")
        os.makedirs(os.path.join(cls.SOURCE_DIR, "subdir"), exist_ok=True)
        for fname in cls.TEST_FILES:
            open(os.path.join(cls.SOURCE_DIR, fname), 'w').close()

    @classmethod
    def _remove_source_directory(cls):
        shutil.rmtree(cls.SOURCE_DIR)

    def test_mount_in_new_folder(self):
        destination_dir = "/user_mount_test"
        source_dir = self.SOURCE_DIR
        sarus_options = ["--mount=type=bind,source=" + source_dir + ",destination=" + destination_dir]
        expected_files = [destination_dir+"/"+fname for fname in self.TEST_FILES]
        self.assertTrue(self._files_exist_in_container(expected_files, sarus_options))

    def test_mount_in_existing_folder(self):
        destination_dir = "/usr/local"
        source_dir = self.SOURCE_DIR
        sarus_options = ["--mount=type=bind,source=" + source_dir + ",destination=" + destination_dir]
        expected_files = [destination_dir+"/"+fname for fname in self.TEST_FILES]
        self.assertTrue(self._files_exist_in_container(expected_files, sarus_options))

    def test_mount_in_dev(self):
        destination_dir = "/dev/unit_test"
        source_dir = self.SOURCE_DIR
        sarus_options = ["--mount=type=bind,source=" + source_dir + ",destination=" + destination_dir]
        expected_files = [destination_dir+"/"+fname for fname in self.TEST_FILES]
        self.assertTrue(self._files_exist_in_container(expected_files, sarus_options))

    def _files_exist_in_container(self, file_paths, sarus_options):
        check_script = self.CHECK_TEMPLATE.format(files=" ".join(['"{}"'.format(fpath) for fpath in file_paths]))
        command = ["bash", "-c"] + [check_script]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.CONTAINER_IMAGE,
                                            command=command,
                                            options_of_run_command=sarus_options)
        return out == ["PASS"]
