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
    _OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/glibc_hook.json"
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
        cls._enable_hook()

    @classmethod
    def tearDownClass(cls):
        cls._disable_hook()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image="alpine:3.8") # no glibc
        util.pull_image_if_necessary(is_centralized_repository=False, image="centos:6") # glibc 2.12
        util.pull_image_if_necessary(is_centralized_repository=False, image="fedora:latest") # assumption: glibc >= host's glibc

    @classmethod
    def _enable_hook(cls):
        # create OCI hook's config file
        hook = dict()
        hook["version"] = "1.0.0"
        hook["hook"] = dict()
        hook["hook"]["path"] = os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/glibc_hook"
        hook["hook"]["env"] = [
            "GLIBC_LIBS=" + ":".join(cls._HOST_GLIBC_LIBS),
            "LDCONFIG_PATH=" + shutil.which("ldconfig"),
            "READELF_PATH=" + shutil.which("readelf"),
        ]
        hook["when"] = dict()
        hook["when"]["annotations"] = {"^com.hooks.glibc.enabled$": "^true$"}
        hook["stages"] = ["prestart"]

        with open("hook_config.json.tmp", 'w') as f:
            f.write(json.dumps(hook))

        subprocess.check_output(["sudo", "mv", "hook_config.json.tmp", cls._OCIHOOK_CONFIG_FILE])
        subprocess.check_output(["sudo", "chown", "root:root", cls._OCIHOOK_CONFIG_FILE])

    @classmethod
    def _disable_hook(cls):
        subprocess.call(["sudo", "rm", cls._OCIHOOK_CONFIG_FILE])

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
