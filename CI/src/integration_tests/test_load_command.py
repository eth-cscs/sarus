import unittest
import os

import common.util as util


class TestLoadCommand(unittest.TestCase):
    """
    These test verify that the Sarus's load command works correctly.
    """

    def test_load_command_with_local_repository(self):
        self._test_load_command(is_centralized_repository=False)

    def test_load_command_with_centralized_repository(self):
        self._test_load_command(is_centralized_repository=True)

    def _test_load_command(self, is_centralized_repository):
        util.remove_image_if_necessary(is_centralized_repository, "load/library/alpine:latest")
        image_name = "alpine"
        image_archive = os.path.dirname(os.path.realpath(__file__)) + "/saved_image.tar"

        util.load_image(is_centralized_repository, image_archive, image_name)

        actual_images = set(util.list_images(is_centralized_repository))
        expected_images = {"load/library/alpine:latest"}
        self.assertTrue(expected_images.issubset(actual_images))

        prettyname = util.run_image_and_get_prettyname(is_centralized_repository, "load/library/alpine:latest")

        self.assertEqual(prettyname, "Alpine Linux")
