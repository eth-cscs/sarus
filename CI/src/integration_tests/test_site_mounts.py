# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import sys
import shutil
import json

import common.util as util


class TestSiteMounts(unittest.TestCase):
    """
    These tests verify that using the siteMounts parameter in sarus.json
    correctly bind-mounts the requested directories into the container.
    """

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
        cls._create_source_directories()
        cls._modify_sarusjson_file()
        cls.container_image = "quay.io/ethcscs/ubuntu:20.04"
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.container_image)

    @classmethod
    def tearDownClass(cls):
        cls._remove_source_directories()
        cls._restore_sarusjson_file()

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

    @classmethod
    def _modify_sarusjson_file(cls):
        cls._sarusjson_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"

        # Create backup
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename, cls._sarusjson_filename+'.bak'])

        # Build site mounts
        sitemounts = []
        for destination_path in cls.TEST_FILES.keys():
            source_path = os.getcwd() + destination_path
            sitemounts.append({"type": "bind", "source": source_path, "destination": destination_path})

        # Modify config file
        with open(cls._sarusjson_filename, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["siteMounts"] = sitemounts
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", cls._sarusjson_filename])
        os.remove("sarus.json.dummy")

    @classmethod
    def _restore_sarusjson_file(cls):
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename+'.bak', cls._sarusjson_filename])

    def setUp(self):
        pass

    def test_sitefs_mounts(self):
        expected_files = []
        for dir,files in self.TEST_FILES.items():
            expected_files.extend([dir+f for f in files])
        self.assertTrue(self._files_exist_in_container(expected_files, []))

    def _files_exist_in_container(self, file_paths, sarus_options):
        file_names = ['"{}"'.format(fpath) for fpath in file_paths]
        check_script = self.__class__.CHECK_TEMPLATE.format(files=" ".join(file_names))
        command = ["bash", "-c"] + [check_script]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.__class__.container_image,
                                            command=command,
                                            options_of_run_command=sarus_options)
        return out == ["PASS"]


if __name__ == "__main__":
    unittest.main()
