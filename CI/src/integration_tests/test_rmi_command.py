import unittest

import common.util as util


class TestRmiCommand(unittest.TestCase):
    """
    These tests verify that the images are correctly removed.
    """

    def test_rmi_command_with_local_repository(self):
        self._test_rmi_command(is_centralized_repository=False)

    def test_rmi_command_with_centralized_repository(self):
        self._test_rmi_command(is_centralized_repository=True)

    def _test_rmi_command(self, is_centralized_repository):
        image = "alpine:latest"

        util.pull_image_if_necessary(is_centralized_repository, image)
        actual_images = set(util.list_images(is_centralized_repository))
        self.assertTrue(image in actual_images)

        util.remove_image_if_necessary(is_centralized_repository, image)
        actual_images = set(util.list_images(is_centralized_repository))
        self.assertTrue(image not in actual_images)
