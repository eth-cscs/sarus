# Sarus
#
# Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import sys
import subprocess
import re

import common.util as util


class TestCpuAffinity(unittest.TestCase):
    """
    These tests verify that the CPU affinity of the host process (Sarus program)
    is also applied to the container process.
    """

    def test_cpu_affinity(self):
        image = "alpine:3.8"
        util.pull_image_if_necessary(is_centralized_repository=False, image=image)

        command = ["cat", "/proc/self/status"]
        out = util.command_output_without_trailing_new_lines(subprocess.check_output(command))
        host_cpus_allowed_list = self._parse_cpus_allowed_list(out)
        only_one_cpu_available = len(host_cpus_allowed_list) == 1

        if only_one_cpu_available:
            print("WARNING: skipping CPU affinity test. Only one CPU"
                  " is available, but at least two are required.")
            return

        cpu = self._parse_first_cpu(host_cpus_allowed_list)

        command = ["taskset", "--cpu-list", cpu,
                   "sarus", "run", image, "cat", "/proc/self/status"]
        out = util.command_output_without_trailing_new_lines(subprocess.check_output(command))
        container_cpus_allowed_list = self._parse_cpus_allowed_list(out)

        self.assertEqual(container_cpus_allowed_list, cpu)

    def _parse_cpus_allowed_list(self, proc_status):
        for line in proc_status:
            if line.startswith("Cpus_allowed_list"):
                return line.split()[1]
        raise Exception("invalid input: no CPUs allowed list found in the input text")

    def _parse_first_cpu(self, cpus_allowed_list):
        m = re.match(r"([0-9]+)[,-].*", cpus_allowed_list)
        cpu = m.group(1)
        assert len(cpu) > 0 and cpu.isdigit()
        return cpu
