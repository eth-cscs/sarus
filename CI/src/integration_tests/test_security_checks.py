# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

from contextlib import contextmanager
import json
import os
import pytest
import stat
import shutil
import subprocess
import unittest


CONFIG_FILENAME = "/opt/sarus/default/etc/sarus.json"
SCHEMA_FILENAME = "/opt/sarus/default/etc/sarus.schema.json"
SARUS_FOO_CMD = ["sarus", "--version"]
NONROOT_ID = 1005


@pytest.mark.asroot
class TestSecurityChecks(unittest.TestCase):
    """
    Check that the Security Checks Sarus performs on the configuration folders behave as expected.
    Security Checks are the first thing we expect Sarus to run before any other command.

    These are integration tests, which take a pre-built Sarus binary to test its behaviour.
    To alter security settings, these test needs to touch the configuration files used by such Sarus.

    For this to happen, we assume two things:
        - Sarus is installed in /opt/sarus/default
        - Tests run in a privileged process able to edit permissions of the files and folders in /opt.
            (hence the pytest mark "asroot")
    """

    def setUp(self):
        # To make sure these tests start from a sane default state
        self._sarus_foo()

    def test_untamperable_config(self):
        self._check_untamperable(CONFIG_FILENAME, "sarus.json")

    def test_untamperable_config_schema(self):
        self._check_untamperable(SCHEMA_FILENAME, "sarus.schema.json")

    def test_disabled_security_checks(self):
        with disabled_security_checks():
            # some tamperable locations are OK...
            with changed_owner("/opt/sarus/default/bin/", NONROOT_ID, NONROOT_ID):
                self._sarus_foo()

            # but not these two...
            self._check_untamperable(CONFIG_FILENAME, "sarus.json")
            self._check_untamperable(SCHEMA_FILENAME, "sarus.schema.json")

    def test_untamperable_binaries(self):
        MKSQ_PATH = shutil.which("mksquashfs")
        INIT_PATH = "/opt/sarus/default/bin/tini-static-amd64"
        RUNC_PATH = "/opt/sarus/default/bin/runc.amd64"

        self._check_untamperable(MKSQ_PATH, MKSQ_PATH)
        self._check_untamperable(INIT_PATH, INIT_PATH)
        self._check_untamperable(RUNC_PATH, RUNC_PATH)

        self._check_untamperable("/opt/sarus/default/bin/", "/opt/sarus/default/bin")

    def test_untamperable_hooks_and_deps(self):
        # ssh is only default hook enabled
        self._check_untamperable("/opt/sarus/default/etc/hooks.d/ssh-hook.json", "/opt/sarus/default/etc/hooks.d/ssh-hook.json")
        self._check_untamperable("/opt/sarus/default/bin/ssh_hook", "/opt/sarus/default/bin/ssh_hook")
        self._check_untamperable("/opt/sarus/default/openssh", "/opt/sarus/default/openssh")


    def _assert_raises(self, command, expected_message_content):
            actual_message = self._get_sarus_error_output(command)
            assert expected_message_content in  actual_message

    def _get_sarus_error_output(self, command):
        with open(os.devnull, 'wb') as devnull:
            proc = subprocess.run(command, stdout=devnull, stderr=subprocess.PIPE)
            if proc.returncode == 0:
                self.fail("Sarus didn't generate any error, but at least one was expected.")

        stderr_without_trailing_whitespaces = proc.stderr.rstrip()
        return stderr_without_trailing_whitespaces.decode()

    def _sarus_foo(self):
        """
        Performs the simplest Sarus action so Security Checks are run.
        """
        with open(os.devnull, 'wb') as devnull:
            res = subprocess.run(SARUS_FOO_CMD, stdout=devnull, check=True)
        assert res.returncode == 0

    def _check_untamperable(self, path, err_substr):
        with changed_owner(path, NONROOT_ID, NONROOT_ID):
            self._assert_raises(SARUS_FOO_CMD, err_substr + '" must be owned by root')

        with changed_permissions(path, stat.S_IWGRP):
            self._assert_raises(SARUS_FOO_CMD, err_substr + '" cannot be group- or world-writable')

        with changed_permissions(path, stat.S_IWOTH):
            self._assert_raises(SARUS_FOO_CMD, err_substr + '" cannot be group- or world-writable')


    def tearDown(self):
        # To make sure these tests are not altering config files permissions
        self._sarus_foo()


@contextmanager
def changed_owner(path, uid, gid):
    """
    Make path owned by uid:gid
    """
    old_uid = os.stat(path).st_uid
    old_gid = os.stat(path).st_gid
    try:
        shutil.chown(path, uid, gid)
        yield
    finally:
        shutil.chown(path, old_uid, old_gid)


@contextmanager
def changed_permissions(path, mod):
    old_mod = os.stat(path).st_mode
    try:
        os.chmod(path, mod)
        yield
    finally:
        os.chmod(path, old_mod)


@contextmanager
def disabled_security_checks():
    shutil.copy2("/opt/sarus/default/etc/sarus.json", "/opt/sarus/default/etc/sarus.json.testbak")
    with open("/opt/sarus/default/etc/sarus.json") as conf_file:
        conf_content = json.load(conf_file)
    try:
        with open("/opt/sarus/default/etc/sarus.json", "w") as conf_file:
            conf_content['securityChecks'] = False
            json.dump(conf_content, conf_file)
        yield
    finally:
        shutil.move("/opt/sarus/default/etc/sarus.json.testbak", "/opt/sarus/default/etc/sarus.json")


if __name__ == '__main__':
    unittest.main()
