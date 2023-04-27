# Sarus
#
# Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import common.util as util
import pytest
import unittest


class TestCommandRun(unittest.TestCase):
    """
    These tests verify that the available images are run correctly,
    i.e. the Linux's pretty name inside the container is correct.
    """

    DEFAULT_IMAGE = util.ALPINE_IMAGE

    def test_command_run_with_local_repository(self):
        self._test_command_run(is_centralized_repository=False)

    def test_command_run_with_centralized_repository(self):
        self._test_command_run(is_centralized_repository=True)

    def test_fallback_on_legacy_default_server(self):
        util.remove_image_if_necessary(is_centralized_repository=False, image="alpine")
        util.pull_image_if_necessary(is_centralized_repository=False, image="index.docker.io/library/alpine")
        assert util.run_image_and_get_prettyname(is_centralized_repository=False, image="alpine").startswith("Alpine Linux")

    def test_workdir(self):
        util.pull_image_if_necessary(is_centralized_repository=True, image=self.DEFAULT_IMAGE)

        # default workdir
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.DEFAULT_IMAGE,
                                            command=["pwd"],
                                            options_of_run_command=None)
        self.assertEqual(out, ["/"])
        # custom workdir
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.DEFAULT_IMAGE,
                                            command=["pwd"],
                                            options_of_run_command=["--workdir=/etc"])
        self.assertEqual(out, ["/etc"])
        # custom non-exising workdir (sarus automatically creates it)
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.DEFAULT_IMAGE,
                                            command=["pwd"],
                                            options_of_run_command=["--workdir=/non-exising-dir-2931"])
        self.assertEqual(out, ["/non-exising-dir-2931"])

    def test_entrypoint_with_option_arguments(self):
        util.pull_image_if_necessary(is_centralized_repository=True, image=self.DEFAULT_IMAGE)

        out = util.run_command_in_container(is_centralized_repository=False,
                                            image=self.DEFAULT_IMAGE,
                                            command=["--option", "arg"],
                                            options_of_run_command=["--entrypoint=echo"])
        self.assertEqual(out, ["--option arg"])

    @pytest.mark.xfail(reason="CONTAINER-666")
    def test_private_pid_namespace(self):
        processes = self._run_ps_in_container(with_private_pid_namespace=True, with_init_process=False)
        self.assertEqual(len(processes), 1)
        self.assertEqual(processes[0], {"pid": 1, "comm": "ps"})

    @pytest.mark.xfail(reason="CONTAINER-666")
    def test_init_process(self):
        processes = self._run_ps_in_container(with_private_pid_namespace=True, with_init_process=True)
        self.assertEqual(len(processes), 2)
        self.assertEqual(processes[0], {"pid": 1, "comm": "init"})
        self.assertEqual(processes[1]["comm"], "ps")

    def test_cleanup_image_without_backing_file(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self.DEFAULT_IMAGE)
        util.remove_image_backing_file(self.DEFAULT_IMAGE)

        command = ["sarus", "run", self.DEFAULT_IMAGE, "true"]
        output = util.get_sarus_error_output(command)
        assert f"Image {self.DEFAULT_IMAGE} is not available" in output
        assert self._is_repository_metadata_owned_by_user()

        # Check we can run after pulling again
        util.pull_image(is_centralized_repository=False, image=self.DEFAULT_IMAGE)
        assert util.run_image_and_get_prettyname(is_centralized_repository=False,
                                                 image=self.DEFAULT_IMAGE).startswith("Alpine Linux")

    def _test_command_run(self, is_centralized_repository):
        images = {"quay.io/ethcscs/alpine:3.14": "Alpine Linux",
                  "quay.io/ethcscs/debian:buster": "Debian GNU/Linux",
                  "quay.io/ethcscs/centos:7": "CentOS Linux",
                  "quay.io/ethcscs/alpine@sha256:1775bebec23e1f3ce486989bfc9ff3c4e951690df84aa9f926497d82f2ffca9d":
                      "Alpine Linux",
                  "quay.io/ethcscs/ubuntu:20.04@sha256:778fdd9f62a6d7c0e53a97489ab3db17738bc5c1acf09a18738a2a674025eae6":
                      "Ubuntu"
                 }
        for reference, prettyname in images.items():
            util.pull_image_if_necessary(is_centralized_repository, reference)
            assert util.run_image_and_get_prettyname(is_centralized_repository, reference).startswith(prettyname)

    def _run_ps_in_container(self, with_private_pid_namespace, with_init_process):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self.DEFAULT_IMAGE)
        options = []
        if with_private_pid_namespace:
            options += ['--pid=private']
        if with_init_process:
            options += ["--init"]
        out = util.run_command_in_container(is_centralized_repository=False,
                                            image="quay.io/ethcscs/ubuntu:20.04",
                                            command=["ps", "-o", "pid,comm"],
                                            options_of_run_command=options)
        processes = []
        for line in out[1:]:
            pid, comm = line.split()
            processes.append({"pid": int(pid), "comm": comm})

        processes.sort(key=lambda process: process["pid"])  # sort by pid in ascending order

        return processes

    def _is_repository_metadata_owned_by_user(self):
        import os, pathlib
        repository_metadata = pathlib.Path(util.get_local_repository_path(), "metadata.json")
        metadata_stat = repository_metadata.stat()
        return metadata_stat.st_uid == os.getuid() and metadata_stat.st_gid == os.getgid()
