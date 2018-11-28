import unittest
import subprocess
import os
import sys
import shutil

import common.util as util


class TestUserMounts(unittest.TestCase):
    """
    These tests verify that using the --mount CLI option correctly bind-mounts
    the requested directories into the container.
    """

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
        cls.container_image = "ubuntu:16.04"
        util.pull_image_if_necessary(is_centralized_repository=False,
                                     image=cls.container_image)

    @classmethod
    def tearDownClass(cls):
        cls._remove_source_directory()

    @classmethod
    def _create_source_directory(cls):
        cls.source_dir = os.getcwd() + "/sarus-test/mount_source/"
        os.makedirs(cls.source_dir+"subdir")
        for fname in cls.TEST_FILES:
            open(cls.source_dir+fname, 'w').close()

    @classmethod
    def _remove_source_directory(cls):
        shutil.rmtree(cls.source_dir)

    def setUp(self):
        pass

    def test_mount_in_new_folder(self):
        destination_dir = "/user_mount_test"
        source_dir = self.__class__.source_dir
        sarus_options = ["--mount=type=bind,source=" + source_dir + ",destination=" + destination_dir]
        expected_files = [destination_dir+"/"+fname for fname in self.__class__.TEST_FILES]
        self.assertTrue(self._files_exist_in_container(expected_files, sarus_options))

    def test_mount_in_existing_folder(self):
        destination_dir = "/usr/local"
        source_dir = self.__class__.source_dir
        sarus_options = ["--mount=type=bind,source=" + source_dir + ",destination=" + destination_dir]
        expected_files = [destination_dir+"/"+fname for fname in self.__class__.TEST_FILES]
        self.assertTrue(self._files_exist_in_container(expected_files, sarus_options))

    def _files_exist_in_container(self, file_paths, sarus_options):
        check_script = self.__class__.CHECK_TEMPLATE.format(files=" ".join(['"{}"'.format(fpath) for fpath in file_paths]))
        command = ["bash", "-c"] + [check_script]
        out = util.run_command_in_container(is_centralized_repository=False,
                                                   image=self.__class__.container_image,
                                                   command=command,
                                                   options_of_run_command=sarus_options)
        return out == ["PASS"]


if __name__ == "__main__":
    unittest.main()