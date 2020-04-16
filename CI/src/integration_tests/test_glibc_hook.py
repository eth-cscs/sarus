# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import shutil
import json

import common.util as util

def _get_glibc_path():
    if os.path.isfile("/lib/x86_64-linux-gnu/libc.so.6"):
        return "/lib/x86_64-linux-gnu"
    elif os.path.isfile("/lib64/libc.so.6"):
        return "/lib64"
    else:
        raise Exception("Could not find glibc on the system. Hint: update this"
                        " test code with the glibc's path on this system")

class TestGlibcHook(unittest.TestCase):
    """
    These tests verify that the host Glibc libraries are properly brought into the container.
    """
    _SARUS_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"
    _HOST_GLIBC_LIBS = [
        os.path.join(_get_glibc_path(), lib) for lib in (
            "ld-linux-x86-64.so.2",
            "libBrokenLocale.so.1",
            "libSegFault.so",
            "libanl.so.1",
            "libc.so.6",
            "libcrypt.so.1",
            "libdl.so.2",
            "libm.so.6",
            "libnsl.so.1",
            "libnss_dns.so.2",
            "libnss_files.so.2",
            "libnss_hesiod.so.2",
            "libpthread.so.0",
            "libresolv.so.2",
            "librt.so.1",
            "libthread_db.so.1",
            "libutil.so.1")]
    _HOST_GLIBC_LIBS_HASHES = [subprocess.check_output(["md5sum", lib]).decode().split()[0] for lib in _HOST_GLIBC_LIBS]

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._modify_bundle_config()

    @classmethod
    def tearDownClass(cls):
        cls._undo_modify_bundle_config()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image="alpine:3.8") # no glibc
        util.pull_image_if_necessary(is_centralized_repository=False, image="centos:6") # glibc 2.12
        util.pull_image_if_necessary(is_centralized_repository=False, image="fedora:latest") # assumption: glibc >= host's glibc

    @classmethod
    def _modify_bundle_config(cls):
        # make backup of config file
        subprocess.call(["sudo", "cp", cls._SARUS_CONFIG_FILE, cls._SARUS_CONFIG_FILE + ".bak"])

        # Build OCIHooks object
        glibc_hook = dict()
        glibc_hook["path"] = os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/glibc_hook"
        glibc_hook["env"] = list()
        glibc_hook["env"].append("GLIBC_LIBS=" + ":".join(cls._HOST_GLIBC_LIBS))
        glibc_hook["env"].append("LDCONFIG_PATH=" + shutil.which("ldconfig"))
        glibc_hook["env"].append("READELF_PATH=" + shutil.which("readelf"))

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

    def setUp(self):
        self._glibc_command_line_option = None
        self._mpi_command_line_option = None

    def test_container_without_glibc(self):
        self.assertEqual(
            util.run_image_and_get_prettyname(is_centralized_repository=False, image="alpine:3.8"),
            "Alpine Linux")

    def test_no_injection_in_container_with_recent_glibc(self):
        self._glibc_command_line_option = True
        self._container_image = "fedora:latest"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def test_injection_in_container_with_old_glibc(self):
        self._glibc_command_line_option = True
        self._container_image = "centos:6"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert hashes
        assert hashes.issuperset(self._HOST_GLIBC_LIBS_HASHES)

    def test_injection_in_container_with_old_glibc_using_mpi_option(self):
        self._glibc_command_line_option = False
        self._mpi_command_line_option = True
        self._container_image = "centos:6"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert hashes
        assert hashes.issuperset(self._HOST_GLIBC_LIBS_HASHES)

    def test_no_hook_activation(self):
        self._glibc_command_line_option = False
        self._mpi_command_line_option = False
        self._container_image = "centos:6"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def _get_hashes_of_host_libs_in_container(self):
        options = []
        if self._glibc_command_line_option:
            options.append("--glibc")
        if self._mpi_command_line_option:
            options.append("--mpi")
        hashes = util.get_hashes_of_host_libs_in_container(is_centralized_repository=False,
                                                           image=self._container_image,
                                                           options_of_run_command=options)
        return set(hashes)
