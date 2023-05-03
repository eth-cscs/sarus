# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import subprocess
import os
import shutil

import common.util as util

def _get_glibc_path():
    if os.path.isfile("/lib/x86_64-linux-gnu/libc.so.6"):
        return "/lib/x86_64-linux-gnu"
    elif os.path.isfile("/lib64/libc.so.6"):
        return "/lib64"
    else:
        raise Exception("Could not find glibc on the system. Hint: update this"
                        " test code with the glibc's path on this system")

def _get_distro_id():
    with open('/etc/os-release') as f:
        for line in f.readlines():
            if line.startswith("ID"):
                return line.split("=")[1].strip('"\n')

def _get_distro_version_id():
    with open('/etc/os-release') as f:
        for line in f.readlines():
            if line.startswith("VERSION_ID"):
                return line.split("=")[1].strip('"\n')

def _get_host_glibc_libs():
    libs = [
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
    # patch distro-specific lib paths
    if _get_distro_id() == "opensuse-leap" and float(_get_distro_version_id()) >= 15.3:
        # OpenSUSE Leap 15.3 provides libcrypt through its own package, installing
        # files in /usr/lib64:
        # https://opensuse.pkgs.org/15.3/opensuse-oss-x86_64/libcrypt1-4.4.15-2.51.x86_64.rpm.html
        # Leap 15.2 still bundles libcrypt with glibc
        libs.remove(os.path.join(_get_glibc_path(), "libcrypt.so.1"))
        libs.append("/usr/lib64/libcrypt.so.1")
    if _get_distro_id() == "fedora":
        if int(_get_distro_version_id()) >= 28:
            # libnsl.so.1 is no longer part of the glibc package and no longer installed by default since F28:
            # https://fedoraproject.org/wiki/Releases/28/ChangeSet#NIS_switching_to_new_libnsl_to_support_IPv6
            # Fedora >=28 ships with /usr/lib64/libnsl.so.2 (from the libnsl2 RPM, e.g.
            # https://fedora.pkgs.org/34/fedora-x86_64/libnsl2-1.3.0-2.fc34.x86_64.rpm.html).
            # The RPM description states that "This code was formerly part of glibc, but is now standalone to
            # be able to link against TI-RPC for IPv6 support."
            libs.remove(os.path.join(_get_glibc_path(), "libnsl.so.1"))
            libs.append("/usr/lib64/libnsl.so.2")
            # Fedora does not install by default libnss_hesiod.so.2 at least since Fedora 28 (could be earlier)
            libs.remove(os.path.join(_get_glibc_path(), "libnss_hesiod.so.2"))
        if int(_get_distro_version_id()) >= 36:
            libs.remove(os.path.join(_get_glibc_path(), "libSegFault.so"))
            libs.remove("/usr/lib64/libnsl.so.2")
            libs.extend(["/usr/lib64/libsigsegv.so.2", "/usr/lib64/libnsl.so.3"])
    if _get_distro_id() == "rocky":
        if float(_get_distro_version_id()) >= 8:
            libs.remove(os.path.join(_get_glibc_path(), "libnsl.so.1"))
            libs.append("/usr/lib64/libnsl.so.2")
            libs.remove(os.path.join(_get_glibc_path(), "libnss_hesiod.so.2"))
        if float(_get_distro_version_id()) >= 9:
            libs.remove(os.path.join(_get_glibc_path(), "libSegFault.so"))
            libs.remove("/usr/lib64/libnsl.so.2")
            libs.append("/usr/lib64/libsigsegv.so.2")
    if _get_distro_id() == "ubuntu" and float(_get_distro_version_id()) >= 22.04:
        libs.remove(os.path.join(_get_glibc_path(), "libSegFault.so"))
        libs.append("/lib/x86_64-linux-gnu/libsigsegv.so.2")
    return libs


class TestGlibcHook(unittest.TestCase):
    """
    These tests verify that the host Glibc libraries are properly brought into the container.
    """
    _OCIHOOK_CONFIG_FILE = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/hooks.d/glibc_hook.json"
    _HOST_GLIBC_LIBS = _get_host_glibc_libs()
    _HOST_GLIBC_LIBS_HASHES = [util.generate_file_md5_hash(lib, "md5") for lib in _HOST_GLIBC_LIBS]

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._enable_hook()

    @classmethod
    def tearDownClass(cls):
        cls._disable_hook()

    @classmethod
    def _pull_docker_images(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=util.ALPINE_IMAGE)            # no glibc
        util.pull_image_if_necessary(is_centralized_repository=False, image="quay.io/ethcscs/centos:6")   # glibc 2.12
        util.pull_image_if_necessary(is_centralized_repository=False, image="quay.io/ethcscs/fedora:36")  # assumption: glibc >= host's glibc
        # based on fedora - assumption: glibc >= host's glibc
        util.pull_image_if_necessary(is_centralized_repository=False, image="quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry_f36")

    @classmethod
    def _enable_hook(cls):
        hook = {
            "version": "1.0.0",
            "hook": {
                "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/glibc_hook",
                "env": [
                    "GLIBC_LIBS=" + ":".join(cls._HOST_GLIBC_LIBS),
                    "LDD_PATH=" + shutil.which("ldd"),
                    "LDCONFIG_PATH=" + shutil.which("ldconfig"),
                    "READELF_PATH=" + shutil.which("readelf"),
                ]
            },
            "when": {
                "annotations": {
                    "^com.hooks.glibc.enabled$": "^true$"
                }
            },
            "stages": ["prestart"]
        }

        util.create_hook_file(hook, cls._OCIHOOK_CONFIG_FILE)

    @classmethod
    def _disable_hook(cls):
        subprocess.call(["sudo", "rm", cls._OCIHOOK_CONFIG_FILE])

    def setUp(self):
        self._glibc_command_line_option = None
        self._mpi_command_line_option = None

    def test_container_without_glibc(self):
        prettyname = util.run_image_and_get_prettyname(is_centralized_repository=False,
                                                       image=util.ALPINE_IMAGE)
        assert prettyname.startswith("Alpine Linux")

    def test_no_injection_in_container_with_recent_glibc(self):
        self._glibc_command_line_option = True
        self._container_image = "quay.io/ethcscs/fedora:36"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def test_no_injection_in_container_with_recent_glibc_and_nonexisting_ldcache_entry(self):
        self._glibc_command_line_option = True
        self._container_image = "quay.io/ethcscs/sarus-integration-tests:nonexisting_ldcache_entry_f36"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert not hashes

    def test_injection_in_container_with_old_glibc(self):
        self._glibc_command_line_option = True
        self._container_image = "quay.io/ethcscs/centos:6"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert hashes
        assert hashes.issuperset(self._HOST_GLIBC_LIBS_HASHES)

    def test_injection_in_container_with_old_glibc_using_mpi_option(self):
        self._glibc_command_line_option = False
        self._mpi_command_line_option = True
        self._container_image = "quay.io/ethcscs/centos:6"
        hashes = self._get_hashes_of_host_libs_in_container()
        assert hashes
        assert hashes.issuperset(self._HOST_GLIBC_LIBS_HASHES)

    def test_no_hook_activation(self):
        self._glibc_command_line_option = False
        self._mpi_command_line_option = False
        self._container_image = "quay.io/ethcscs/centos:6"
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
