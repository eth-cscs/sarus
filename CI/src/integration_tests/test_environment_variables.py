# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import unittest
import os
import subprocess

import common.util as util


class TestEnvironmentVariables(unittest.TestCase):
    """
    These tests verify that the environment variables are
    properly set inside the container by Sarus.
    """

    _IMAGE_NAME = "quay.io/ethcscs/sarus-integration-tests:environment-variables"

    @classmethod
    def setUpClass(cls):
        cls._modify_sarusjson_file()
        util.pull_image_if_necessary(is_centralized_repository=False, image=cls._IMAGE_NAME)

    @classmethod
    def tearDownClass(cls):
        cls._restore_sarusjson_file()

    @classmethod
    def _modify_sarusjson_file(cls):
        cls._sarusjson_filename = os.environ["CMAKE_INSTALL_PREFIX"] + "/etc/sarus.json"

        # Create backup
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename, cls._sarusjson_filename+'.bak'])

        # Build config environment
        environment = {
            "set": {
                "SARUS_CONFIG_SET": "config_set_value"
            },
            "prepend": {
                "IMAGE_ENV_VARIABLE": "config_prepend_value0",
                "HOST_ENV_VAR": "config_prepand_value1",
                "SARUS_CONFIG_PREPEND": "config_prepend_value2"
            },
            "append": {
                "IMAGE_ENV_VARIABLE": "config_append_value0",
                "HOST_ENV_VARIABLE": "config_append_value1",
                "SARUS_CONFIG_APPEND": "config_append_value2"
            },
            "unset": [
                "SARUS_CONFIG_UNSET",
                "IMAGE_ENV_VAR"
            ]
        }

        # Modify config file
        import json
        with open(cls._sarusjson_filename, 'r') as f:
            data = f.read().replace('\n', '')
        config = json.loads(data)
        config["environment"] = environment
        data = json.dumps(config)
        with open("sarus.json.dummy", 'w') as f:
            f.write(data)
        subprocess.check_output(["sudo", "cp", "sarus.json.dummy", cls._sarusjson_filename])
        os.remove("sarus.json.dummy")

    @classmethod
    def _restore_sarusjson_file(cls):
        subprocess.check_output(["sudo", "cp", cls._sarusjson_filename+'.bak', cls._sarusjson_filename])

    def test_environment_variables(self):
        host_environment = os.environ.copy()
        host_environment["IMAGE_ENV_VARIABLE"] = "image_env_variable_value_from_host"
        host_environment["HOST_ENV_VARIABLE"] = "host_env_variable_value"
        host_environment["HOST_ENV_VAR"] = "host_env_var_value"
        host_environment["SARUS_CONFIG_UNSET"] = "config_unset_value"

        command=["bash", "-c", "echo $IMAGE_ENV_VARIABLE; echo $IMAGE_ENV_VAR; echo $HOST_ENV_VARIABLE; echo $HOST_ENV_VAR; "
                               "echo $SARUS_CONFIG_SET; echo $SARUS_CONFIG_PREPEND; echo $SARUS_CONFIG_APPEND; echo $SARUS_CONFIG_UNSET"]

        output = util.run_command_in_container( is_centralized_repository=False,
                                                image=self._IMAGE_NAME,
                                                command=command,
                                                env=host_environment)

        self.assertEqual(output[0], "config_prepend_value0:image_env_variable_value:config_append_value0")
        self.assertEqual(output[1], "")
        self.assertEqual(output[2], "host_env_variable_value:config_append_value1")
        self.assertEqual(output[3], "config_prepand_value1:host_env_var_value")
        self.assertEqual(output[4], "config_set_value")
        self.assertEqual(output[5], "config_prepend_value2")
        self.assertEqual(output[6], "config_append_value2")
        self.assertEqual(len(output), 7)

    def test_env_cli_option(self):
        host_environment = os.environ.copy()
        host_environment["SARUS_CONFIG_UNSET"] = "config_unset_value"
        host_environment["IMAGE_ENV_VAR"] = "image_env_var_value_from_host"

        sarus_options = ["-e", "IMAGE_ENV_VARIABLE=cli_value",
                         "--env", "IMAGE_ENV_VAR",
                         "--env", "EMPTY_VAR=",
                         "--env=SARUS_CONFIG_UNSET=cli_value_reset"]

        command=["bash", "-c", "echo $IMAGE_ENV_VARIABLE; echo $IMAGE_ENV_VAR; echo $EMPTY_VAR; echo $SARUS_CONFIG_UNSET"]

        output = util.run_command_in_container( is_centralized_repository=False,
                                                image=self._IMAGE_NAME,
                                                command=command,
                                                options_of_run_command=sarus_options,
                                                env=host_environment)

        self.assertEqual(output[0], "cli_value")
        self.assertEqual(output[1], "image_env_var_value_from_host")
        self.assertEqual(output[2], "")
        self.assertEqual(output[3], "cli_value_reset")
        self.assertEqual(len(output), 4)
