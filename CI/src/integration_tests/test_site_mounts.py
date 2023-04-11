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


class TestSiteMounts(unittest.TestCase):
    """
    These tests verify that using the siteMounts parameter in sarus.json
    correctly bind-mounts the requested directories into the container.
    """

    CONTAINER_IMAGE = util.UBUNTU_IMAGE
    TEST_FILES = {"/test_sitefs/": ["a.txt", "b.md", "c.py", "subdir/d.cpp", "subdir/e.pdf"],
                  "/test_resources/": ["f.txt", "g.md", "h.py", "subdir/i.cpp", "subdir/j.pdf"],
                  "/test_files/test_dir/": ["k.txt", "m.md", "l.py", "subdir/m.cpp", "subdir/n.pdf"],
                  "/dev/test_site_mount/": ["testdev0"]}
    CHECK_TEMPLATE = ("for fname in {files}; do"
                      "    if [ ! -f $fname ]; then"
                      "         echo \"FAIL\"; "
                      "    fi; "
                      "done; "
                      "echo \"PASS\" ")

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)
        cls._create_source_directories()

        site_mounts = []
        for destination_path in cls.TEST_FILES.keys():
            source_path = os.getcwd() + destination_path
            site_mounts.append({"type": "bind", "source": source_path, "destination": destination_path})
        util.modify_sarus_json({"siteMounts": site_mounts})

    @classmethod
    def tearDownClass(cls):
        cls._remove_source_directories()
        util.restore_sarus_json()

    @classmethod
    def _create_source_directories(cls):
        for k, v in cls.TEST_FILES.items():
            source_dir = os.getcwd() + k
            os.makedirs(os.path.join(source_dir, "subdir"), exist_ok=True)
            for fname in v:
                open(source_dir+fname, 'w').close()

    @classmethod
    def _remove_source_directories(cls):
        for dir in cls.TEST_FILES.keys():
            source_dir = "{}/{}".format(os.getcwd(), dir.split("/")[1])
            shutil.rmtree(source_dir)

    def test_sitefs_mounts(self):
        expected_files = []
        for dir,files in self.TEST_FILES.items():
            expected_files.extend([dir+f for f in files])
        self.assertTrue(self._files_exist_in_container(expected_files, []))

    def _files_exist_in_container(self, file_paths, sarus_options):
        file_names = ['"{}"'.format(fpath) for fpath in file_paths]
        check_script = self.CHECK_TEMPLATE.format(files=" ".join(file_names))
        command = ["bash", "-c"] + [check_script]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.CONTAINER_IMAGE,
                                            command=command,
                                            options_of_run_command=sarus_options)
        return out == ["PASS"]
