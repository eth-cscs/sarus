import unittest

import common.util as util


class TestPullCommand(unittest.TestCase):
    """
    These tests verify that the pulled images are available to the user,
    i.e. correctly listed through the "images" command.
    """

    def test_pull_command_with_local_repository(self):
        self._test_pull_command(is_centralized_repository=False)

    def test_pull_command_with_centralized_repository(self):
        self._test_pull_command(is_centralized_repository=True)

    def _test_pull_command(self, is_centralized_repository):
        image = "alpine:latest"

        util.remove_image_if_necessary(is_centralized_repository, image)
        actual_images = set(util.list_images(is_centralized_repository))
        self.assertTrue(image not in actual_images)

        util.pull_image_if_necessary(is_centralized_repository, image)
        actual_images = set(util.list_images(is_centralized_repository))
        self.assertTrue(image in actual_images)
