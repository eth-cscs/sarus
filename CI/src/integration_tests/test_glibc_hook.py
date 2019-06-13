# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import sys
import tempfile
import re
import json
import hashlib

import common.util as util


class TestGlibcHook(unittest.TestCase):
    """
    These tests verify that the host Glibc libraries are properly brought into the container.
    """
    _SARUS_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"
    _HOST_GLIBC_LIBS = [
        "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2",
        "/lib/x86_64-linux-gnu/libBrokenLocale.so.1",
        "/lib/x86_64-linux-gnu/libSegFault.so",
        "/lib/x86_64-linux-gnu/libanl.so.1",
        "/lib/x86_64-linux-gnu/libc.so.6",
        "/lib/x86_64-linux-gnu/libcidn.so.1",
        "/lib/x86_64-linux-gnu/libcrypt.so.1",
        "/lib/x86_64-linux-gnu/libdl.so.2",
        "/lib/x86_64-linux-gnu/libm.so.6",
        "/lib/x86_64-linux-gnu/libnsl.so.1",
        "/lib/x86_64-linux-gnu/libnss_dns.so.2",
        "/lib/x86_64-linux-gnu/libnss_files.so.2",
        "/lib/x86_64-linux-gnu/libnss_hesiod.so.2",
        "/lib/x86_64-linux-gnu/libpthread.so.0",
        "/lib/x86_64-linux-gnu/libresolv.so.2",
        "/lib/x86_64-linux-gnu/librt.so.1",
        "/lib/x86_64-linux-gnu/libthread_db.so.1",
        "/lib/x86_64-linux-gnu/libutil.so.1"
    ]
    _HOST_GLIBC_LIBS_HASHES = [subprocess.check_output(["md5sum", lib]).split()[0] for lib in _HOST_GLIBC_LIBS]

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._modify_bundle_config()

    @classmethod
    def tearDownClass(cls):
        cls._undo_modify_bundle_config()

    @classmethod
    def _pull_docker_images(cls):
        # note: host is ubuntu 18.04 (glibc 2.27)
        util.pull_image_if_necessary(is_centralized_repository=False, image="centos:7") # glibc 2.17
        util.pull_image_if_necessary(is_centralized_repository=False, image="ubuntu:18.04") # glibc 2.27

    @classmethod
    def _modify_bundle_config(cls):
        # make backup of config file
        subprocess.call(["sudo", "cp", cls._SARUS_CONFIG_FILE, cls._SARUS_CONFIG_FILE + ".bak"])

        # Build OCIHooks object
        glibc_hook = dict()
        glibc_hook["path"] = os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/glibc_hook"
        glibc_hook["env"] = list()
        glibc_hook["env"].append("SARUS_GLIBC_LIBS=" + ":".join(cls._HOST_GLIBC_LIBS))
        glibc_hook["env"].append("SARUS_GLIBC_LDCONFIG_PATH=ldconfig")
        glibc_hook["env"].append("SARUS_GLIBC_READELF_PATH=readelf")

        hooks = dict()
        hooks["prestart"] = [glibc_hook]
        
        # Modify config file
        with open(cls._SARUS_CONFIG_FILE, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["OCIHooks"] = hooks
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", cls._SARUS_CONFIG_FILE])
        os.remove("sarus.json.dummy")

    @classmethod
    def _undo_modify_bundle_config(cls):
        subprocess.call(["sudo", "mv", cls._SARUS_CONFIG_FILE + ".bak", cls._SARUS_CONFIG_FILE])

    def test_container_without_glibc(self):
        self.assertEqual(
            util.run_image_and_get_prettyname(is_centralized_repository=False, image="library/alpine:3.8"),
            "Alpine Linux")

    def test_no_glibc_injection(self):
        self._container_image = "ubuntu:18.04"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def test_glibc_injection(self):
        self._container_image = "centos:7"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert hashes
        assert hashes.issuperset(self._HOST_GLIBC_LIBS_HASHES)

    def _get_hashes_of_host_libs_in_container(self):
        output = util.run_command_in_container( is_centralized_repository=False,
                                                image=self._container_image,
                                                command=["mount"])
        libs = []
        for line in output:
            if re.search(r".* on .*lib.*\.so(\.[0-9]+)* .*", line):
                lib = re.sub(r".* on (.*lib.*\.so(\.[0-9]+)*) .*", r"\1", line)
                libs.append(lib)

        hashes = set()
        for lib in libs:
            output = util.run_command_in_container( is_centralized_repository=False,
                                                    image=self._container_image,
                                                    command=["md5sum", lib])
            hashes.add(output[0].split()[0])

        return hashes
