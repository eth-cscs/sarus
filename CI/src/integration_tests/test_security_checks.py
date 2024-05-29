# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
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

import common.util as util


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
        sarus_foo()

    def tearDown(self):
        # To make sure these tests are not altering config files permissions
        sarus_foo()

    def test_untamperable_config(self):
        check_untamperable(CONFIG_FILENAME)

    def test_untamperable_config_schema(self):
        check_untamperable(SCHEMA_FILENAME)

    def test_disabled_security_checks(self):
        with util.custom_sarus_json({"securityChecks": False}):
            # some tamperable locations are OK...
            with changed_owner("/opt/sarus/default/bin/", NONROOT_ID, NONROOT_ID):
                sarus_foo()

            # but not these two...
            check_untamperable(CONFIG_FILENAME)
            check_untamperable(SCHEMA_FILENAME)

    def test_untamperable_binaries(self):
        installation_prefix = get_parameter_from_config_file("prefixDir")
        init_path = get_parameter_from_config_file("initPath")
        runc_path = get_parameter_from_config_file("runcPath")

        check_untamperable(init_path)
        check_untamperable(runc_path)

        check_untamperable(f"{installation_prefix}/bin")

    def test_untamperable_hooks_and_deps(self):
        hook_config = {
            "version": "1.0.0",
            "hook": {
                "path": os.environ["CMAKE_INSTALL_PREFIX"] + "/bin/timestamp_hook"
            },
            "when": {
                "always": True
            },
            "stages": ["createContainer"]
        }
        hook_config_file = "/opt/sarus/default/etc/hooks.d/00-timestamp-hook.json"
        with util.temporary_hook_files((hook_config, hook_config_file)):
            check_untamperable(hook_config_file)
            check_untamperable("/opt/sarus/default/bin/timestamp_hook")
        check_untamperable("/opt/sarus/default/dropbear")


def get_parameter_from_config_file(parameter):
    with open(CONFIG_FILENAME) as conf_file:
        conf = json.load(conf_file)
        return conf[parameter]


def sarus_foo():
    """
    Performs the simplest Sarus action so Security Checks are run.
    """
    with open(os.devnull, 'wb') as devnull:
        res = subprocess.run(SARUS_FOO_CMD, stdout=devnull, check=True)
    assert res.returncode == 0


def check_untamperable(path):
    if os.path.isdir(path):
        entry = path if path[-1] != '/' else path[:-1]
    else:
        entry = os.path.basename(path)

    with changed_owner(path, NONROOT_ID, NONROOT_ID):
        util.assert_sarus_raises_error_containing_text(SARUS_FOO_CMD,
                                                       '{}" must be owned by root'.format(entry))

    with changed_permissions(path, stat.S_IWGRP):
        util.assert_sarus_raises_error_containing_text(SARUS_FOO_CMD,
                                                       '{}" cannot be group- or world-writable'.format(entry))

    with changed_permissions(path, stat.S_IWOTH):
        util.assert_sarus_raises_error_containing_text(SARUS_FOO_CMD,
                                                       '{}" cannot be group- or world-writable'.format(entry))


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
