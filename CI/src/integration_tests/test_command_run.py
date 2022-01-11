# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import common.util as util


class TestCommandRun(unittest.TestCase):
    """
    These tests verify that the available images are run correctly,
    i.e. the Linux's pretty name inside the container is correct.
    """

    default_image = "quay.io/ethcscs/alpine:3.14"

    def test_command_run_with_local_repository(self):
        self._test_command_run(is_centralized_repository=False)

    def test_command_run_with_centralized_repository(self):
        self._test_command_run(is_centralized_repository=True)

    def test_workdir(self):
        util.pull_image_if_necessary(is_centralized_repository=True, image=self.default_image)

        # default workdir
        out = util.run_command_in_container(is_centralized_repository=False,
                                    image=self.default_image,
                                    command=["pwd"],
                                    options_of_run_command=None)
        self.assertEqual(out, ["/"])
        # custom workdir
        out = util.run_command_in_container(is_centralized_repository=False,
                                    image=self.default_image,
                                    command=["pwd"],
                                    options_of_run_command=["--workdir=/etc"])
        self.assertEqual(out, ["/etc"])
        # custom non-exising workdir (sarus automatically creates it)
        out = util.run_command_in_container(is_centralized_repository=False,
                                    image=self.default_image,
                                    command=["pwd"],
                                    options_of_run_command=["--workdir=/non-exising-dir-2931"])
        self.assertEqual(out, ["/non-exising-dir-2931"])

    def test_entrypoint_with_option_arguments(self):
        util.pull_image_if_necessary(is_centralized_repository=True, image=self.default_image)

        out = util.run_command_in_container(is_centralized_repository=False,
                                    image=self.default_image,
                                    command=["--option", "arg"],
                                    options_of_run_command=["--entrypoint=echo"])
        self.assertEqual(out, ["--option arg"])

    def test_private_pid_namespace(self):
        processes = self._run_ps_in_container(with_private_pid_namespace=True, with_init_process=False)
        self.assertEqual(len(processes), 1)
        self.assertEqual(processes[0], {"pid": 1, "comm": "ps"})

    def test_init_process(self):
        processes = self._run_ps_in_container(with_private_pid_namespace=True, with_init_process=True)
        self.assertEqual(len(processes), 2)
        self.assertEqual(processes[0], {"pid": 1, "comm": "init"})
        self.assertEqual(processes[1]["comm"], "ps")

    def _test_command_run(self, is_centralized_repository):
        util.pull_image_if_necessary(is_centralized_repository, "quay.io/ethcscs/alpine:3.14")
        util.pull_image_if_necessary(is_centralized_repository, "quay.io/ethcscs/debian:buster")
        util.pull_image_if_necessary(is_centralized_repository, "quay.io/ethcscs/centos:7")

        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "quay.io/ethcscs/alpine:3.14"),
            "Alpine Linux")
        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "quay.io/ethcscs/debian:buster"),
            "Debian GNU/Linux 10 (buster)")
        self.assertEqual(util.run_image_and_get_prettyname(is_centralized_repository, "quay.io/ethcscs/centos:7"),
            "CentOS Linux")

    def _run_ps_in_container(self, with_private_pid_namespace, with_init_process):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self.default_image)
        options = []
        if with_private_pid_namespace:
            options += ['--pid=private']
        if with_init_process:
            options += ["--init"]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.default_image,
                                            command=["ps", "-o", "pid,comm"],
                                            options_of_run_command=options)
        processes = []
        for line in out[1:]:
            pid, comm = line.split()
            processes.append({"pid": int(pid), "comm": comm})

        processes.sort(key = lambda process: process["pid"]) # sort by pid in ascending order

        return processes
