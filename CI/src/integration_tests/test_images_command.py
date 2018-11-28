import unittest
import subprocess
import os


import common.util as util


class TestImagesCommand(unittest.TestCase):
    """
    This test checks the output of the "images" command in different scenarios:
    - when the metadata file doesn't exist.
    - when the metadata file contains images.
    - when the metadata file is empty.
    """

    _IMAGE_NAME = "loaded_image:latest"

    def test_images_command_with_local_repository(self):
        self._test_images_command(is_centralized_repository=False)

    def test_images_command_with_centralized_repository(self):
        self._test_images_command(is_centralized_repository=True)

    def _test_images_command(self, is_centralized_repository):
        expected_header = ["REPOSITORY", "TAG", "DIGEST", "CREATED", "SIZE", "SERVER"]

        # import image (with fixed digest)
        image_archive = os.path.dirname(os.path.abspath(__file__)) + "/saved_image.tar"
        util.load_image(is_centralized_repository, image_archive, self._IMAGE_NAME)

        # header
        actual_header = self._header_in_output_of_images_command(is_centralized_repository)
        self.assertEqual(actual_header, expected_header)

        # imported image
        image_output = self._image_in_output_of_images_command(is_centralized_repository, "library/loaded_image", "latest")
        self.assertEqual(image_output[0], "library/loaded_image")
        self.assertEqual(image_output[1], "latest")
        self.assertEqual(image_output[2], "e21c333399e0")
        self.assertEqual(image_output[4], "1.91MB")
        self.assertEqual(image_output[5], "load")

    def _header_in_output_of_images_command(self, is_centralized_repository):
        return self._images_command_output(is_centralized_repository)[0]

    def _image_in_output_of_images_command(self, is_centralized_repository, image, tag):
        output = self._images_command_output(is_centralized_repository)
        for line in output:
            if line[0] == image and line[1] == tag:
                return line

    def _images_command_output(self, is_centralized_repository):
        if is_centralized_repository:
            command = ["sarus", "images", "--centralized-repository"]
        else:
            command = ["sarus", "images"]
        out = subprocess.check_output(command)
        lines = util.command_output_without_trailing_new_lines(out)
        return [line.split() for line in lines]
