# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os

import common.util as util


class TestEnvironmentVariables(unittest.TestCase):
    """
    These tests verify that the environment variables are
    properly set inside the container by Sarus.
    """

    _IMAGE_NAME = "ethcscs/sarus-integration-tests:environment-variables"

    def test_environment_variables(self):
        util.pull_image_if_necessary(is_centralized_repository=False, image=self._IMAGE_NAME)

        host_environment = os.environ.copy()
        host_environment["HOST_ENV_VARIABLE"] = "host_env_variable_value"
        host_environment["HOST_ENV_VAR"] = "host_env_var_value"

        command=["bash", "-c", "echo $IMAGE_ENV_VARIABLE; echo $IMAGE_ENV_VAR; echo $HOST_ENV_VARIABLE; echo $HOST_ENV_VAR"]

        output = util.run_command_in_container( is_centralized_repository=False,
                                                image=self._IMAGE_NAME,
                                                command=command,
                                                env=host_environment)

        self.assertEqual(output[0], "image_env_variable_value")
        self.assertEqual(output[1], "image_env_var_value")
        self.assertEqual(output[2], "host_env_variable_value")
        self.assertEqual(output[3], "host_env_var_value")
