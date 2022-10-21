# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import os
import time
import psutil
import pytest
import signal
import pathlib
import unittest

import common.util as util


@pytest.mark.asroot
class TestCleanupAfterSignals(unittest.TestCase):
    """
    This test verifies that the child processes of Sarus (i.e. the OCI runtime
    and the container process) and the cgroup filesystem are cleaned up after
    the Sarus process receives a SIGTERM or SIGHUP.

    SIGTERM and SIGHUP should be forwarded all the way to the container
    process, which then terminates and allows the OCI runtime to clean up and
    exit as well. SIGHUP should be used to terminate shell programs, which do
    not react to SIGTERM.
    SIGKILL might be used in extreme situations. There is no way to forward
    such signal, so in this case the OCI runtime should receive the SIGHUP set
    by prctl(2) to be sent upon the death of the parent process (i.e. Sarus).

    These tests need root privileges to terminate Sarus, which runs as a
    root-owned process. For this reason, this test class is marked 'asroot'.

    The tests tearDown() methods attempt to clean up if Sarus is unsuccessful in
    doing so by itself.
    """

    CONTAINER_IMAGE = util.UBUNTU_IMAGE
    cpuset_cgroup_path = pathlib.Path("/sys/fs/cgroup/cpuset")

    @classmethod
    def setUpClass(cls):
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls.CONTAINER_IMAGE)

    def setUp(self):
        self.sarus_children = []

    def tearDown(self):
        # Ensure children cleanup if tests fail
        # According to the psutil docs, the order of the children is determined
        # by a depth-first traversal of the process subtree.
        # The OCI runtime is expected as the only first-level child of Sarus,
        # and as such is always the first element of the list.
        # We clean up the children in reversed order, thus terminating the
        # container processes first, leaving the OCI Runtime last to give
        # it the chance to clean up the cgroup subdir it created.
        for p in reversed(self.sarus_children):
            try:
                self._terminate_or_kill(p)
            except psutil.NoSuchProcess:
                pass

    def test_terminate(self):
        self._run_test(run_options=[], commands=["sleep", "20"], sig=signal.SIGTERM)

    def test_interactive_terminal_hangup(self):
        self._run_test(run_options=["-t"], commands=["bash"], sig=signal.SIGHUP)

    def test_init_process_terminate(self):
        self._run_test(run_options=["--init"], commands=["sleep", "20"], sig=signal.SIGTERM)

    def test_init_and_terminal_hangup(self):
        self._run_test(run_options=["-t", "--init"], commands=["sh"], sig=signal.SIGHUP)

    def _run_test(self, run_options, commands, sig):
        sarus_process = psutil.Popen(["sarus", "run"] + run_options + [self.CONTAINER_IMAGE] + commands)
        time.sleep(2)
        # test the runtime process has been created
        self.assertEqual(len(sarus_process.children()), 1,
                         "Did not find single child process of Sarus")
        self.assertEqual(len(list(self.cpuset_cgroup_path.glob("container-*"))), 1,
                         "Could not find cgroup subdir for the container")
        self.sarus_children = sarus_process.children(recursive=True)

        os.kill(sarus_process.pid, sig)
        time.sleep(1)
        self.assertFalse(any([p.is_running() for p in self.sarus_children]),
                         "Sarus child processes were not cleaned up")
        self.assertFalse(list(self.cpuset_cgroup_path.glob("container-*")),
                         "Cgroup subdir was not cleaned up")

    def _terminate_or_kill(self, process):
        process.terminate()
        try:
            process.wait(0.2)
        except psutil.TimeoutExpired:
            process.kill()
