# Sarus
#
# Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

import os
import re
import shutil
import subprocess


def command_output_without_trailing_new_lines(out):
    lines = [line for line in out.split('\n')]
    # remove empty trailing elements in list (they are caused by trailing new lines)
    while lines and lines[-1] == "":
        lines.pop()
    return lines


def load_image(is_centralized_repository, image_archive, image_name):
    if is_centralized_repository:
        command = ["sarus", "load", "--centralized-repository", image_archive, image_name]
    else:
        command = ["sarus", "load", image_archive, image_name]
    subprocess.check_call(command)


def list_images(is_centralized_repository):
    if is_centralized_repository:
        command = ["sarus", "images", "--centralized-repository"]
    else:
        command = ["sarus", "images"]
    out = subprocess.check_output(command).decode()
    lines = command_output_without_trailing_new_lines(out)
    lines_without_header = lines[1:]
    images = []
    for line in lines_without_header:
        fields = line.split()
        image_repository = fields[0]
        image_tag = fields[1]
        images.append(image_repository + ":" + image_tag)
    return images


def pull_image_if_necessary(is_centralized_repository, image):
    if image in list_images(is_centralized_repository):
        return
    if is_centralized_repository:
        command = ["sarus", "pull", "--centralized-repository", image]
    else:
        command = ["sarus", "pull", image]
    subprocess.check_call(command)


def remove_image_if_necessary(is_centralized_repository, image):
    if image not in list_images(is_centralized_repository):
        return
    if is_centralized_repository:
        command = ["sarus", "rmi", "--centralized-repository", image]
    else:
        command = ["sarus", "rmi", image]
    subprocess.check_call(command)


def generate_ssh_keys(overwrite=False):
    command = ["sarus", "ssh-keygen"]
    if overwrite:
        command.append("--overwrite")
    subprocess.check_call(command)


def run_image_and_get_prettyname(is_centralized_repository, image):
    if is_centralized_repository:
        command = ["sarus", "run", "--centralized-repository", image, "cat", "/etc/os-release"]
    else:
        command = ["sarus", "run", image, "cat", "/etc/os-release"]
    out = subprocess.check_output(command).decode()
    lines = command_output_without_trailing_new_lines(out)
    return lines[0].split("=")[1][1:-1]


def run_command_in_container(is_centralized_repository, image, command, options_of_run_command=None, **subprocess_kwargs):
    full_command = ["sarus", "run"]
    if is_centralized_repository:
        full_command += ["--centralized-repository"]
    if options_of_run_command is not None:
        full_command += options_of_run_command
    full_command += [image] + command

    out = subprocess.check_output(full_command, **subprocess_kwargs).decode()

    return command_output_without_trailing_new_lines(out)


def run_command_in_container_with_slurm(image, command, options_of_srun_command=None,
                                        options_of_run_command=None, **subprocess_kwargs):
    # srun part of the command
    full_command = ["srun"]
    if options_of_srun_command is not None:
        full_command += options_of_srun_command

    # "sarus run" part of the command
    full_command += ["sarus", "run"]
    if options_of_run_command is not None:
        full_command += options_of_run_command
    full_command += [image] + command

    # execute
    out = subprocess.check_output(full_command, **subprocess_kwargs).decode()

    return command_output_without_trailing_new_lines(out)


def get_hashes_of_host_libs_in_container(is_centralized_repository, image, options_of_run_command=None):
    """
    Returns a list of md5sum hashes of dynamic libraries which are bind-mounted into a container from the host.
    This function is used by MPI hook and Glibc hook tests to check libraries replacement.
    """
    output = run_command_in_container(is_centralized_repository=is_centralized_repository,
                                      image=image,
                                      command=["mount"],
                                      options_of_run_command=options_of_run_command)
    libs = []
    for line in output:
        if re.search(r".* on .*lib.*\.so(\.[0-9]+)* .*", line):
            lib = re.sub(r".* on (.*lib.*\.so(\.[0-9]+)*) .*", r"\1", line)
            libs.append(lib)

    hashes = []
    for lib in libs:
        output = run_command_in_container(is_centralized_repository=is_centralized_repository,
                                          image=image,
                                          command=["md5sum", lib],
                                          options_of_run_command=options_of_run_command)
        hashes.append(output[0].split()[0])

    return hashes