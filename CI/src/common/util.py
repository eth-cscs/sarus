# Sarus
#
# Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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


def get_images_command_output(is_centralized_repository, print_digests=False):
    command = ["sarus", "images"]
    if is_centralized_repository:
        command += ["--centralized-repository"]
    if print_digests:
        command += ["--digests"]
    lines = get_trimmed_output(command)
    return [line.split() for line in lines]


def list_images(is_centralized_repository, with_digests=False):
    lines = get_images_command_output(is_centralized_repository, with_digests)
    lines_without_header = lines[1:]
    images = []
    for line in lines_without_header:
        image_repository = line[0]
        image_tag = line[1]
        image_entry = f"{image_repository}:{image_tag}"

        if with_digests:
            image_digest = line[2]
            image_entry += f"@{image_digest}"

        images.append(image_entry)
    return images


def is_image_available(is_centralized_repository, target_image):
    target_digest = target_image.split("@")[1] if "@sha256:" in target_image else None
    if not target_digest:
        if ":" in target_image:
            target_tag = target_image.split(":")[1]
        else:
            target_tag = "latest"
    else:
        target_tag = None

    available_images = get_images_command_output(is_centralized_repository, bool(target_digest))

    for available_entry in available_images:
        available_name, available_tag, available_digest, *rest = available_entry

        if target_image.startswith(available_name):
            if target_tag and available_tag == target_tag:
                return True
            if target_digest and available_tag == "<none>" and available_digest == target_digest:
                return True

    return False


def pull_image_if_necessary(is_centralized_repository, image):
    if is_image_available(is_centralized_repository, image):
        return
    pull_image(is_centralized_repository, image)


def pull_image(is_centralized_repository, image):
    if is_centralized_repository:
        command = ["sarus", "pull", "--centralized-repository", image]
    else:
        command = ["sarus", "pull", image]
    subprocess.check_call(command)


def remove_image_if_necessary(is_centralized_repository, image):
    if not is_image_available(is_centralized_repository, image):
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

    lines = get_trimmed_output(command)
    for line in lines:
        if line.startswith("PRETTY_NAME"):
            return line.split("=")[1].strip('"')


def run_command_in_container(is_centralized_repository, image, command, options_of_run_command=None, **subprocess_kwargs):
    full_command = ["sarus", "run"]
    if is_centralized_repository:
        full_command += ["--centralized-repository"]
    if options_of_run_command is not None:
        full_command += options_of_run_command
    full_command += [image] + command

    return get_trimmed_output(full_command, **subprocess_kwargs)


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

    return get_trimmed_output(full_command, **subprocess_kwargs)


def get_hashes_of_host_libs_in_container(is_centralized_repository, image, options_of_run_command=None):
    """
    Returns a list of md5sum hashes of dynamic libraries which are bind-mounted into a container from the host.
    This function is used by MPI hook and Glibc hook tests to check libraries replacement.
    """
    output = run_command_in_container(is_centralized_repository=is_centralized_repository,
                                      image=image,
                                      command=["cat", "/proc/mounts"],
                                      options_of_run_command=options_of_run_command)
    libs = []
    for line in output:
        if re.search(r".* .*lib.*\.so(\.[0-9]+)* .*", line):
            lib = re.sub(r".* (.*lib.*\.so(\.[0-9]+)*) .*", r"\1", line)
            libs.append(lib)

    hashes = []
    for lib in libs:
        output = run_command_in_container(is_centralized_repository=is_centralized_repository,
                                          image=image,
                                          command=["md5sum", lib],
                                          options_of_run_command=options_of_run_command)
        hashes.append(output[0].split()[0])

    return hashes


def get_trimmed_output(full_command, **subprocess_kwargs):
    try:
        out = subprocess.check_output(full_command, **subprocess_kwargs).decode()
    except subprocess.CalledProcessError as e:
        out = e.output.decode()
        print(out)
        raise
    trimmed_out = command_output_without_trailing_new_lines(out)
    return trimmed_out

def get_sarus_error_output(command):
    with open(os.devnull, 'wb') as devnull:
        proc = subprocess.run(command, stdout=devnull, stderr=subprocess.PIPE)
        if proc.returncode == 0:
            import pytest
            pytest.fail("Sarus didn't generate any error, but at least one was expected.")

        stderr_without_trailing_whitespaces = proc.stderr.rstrip()
        out = stderr_without_trailing_whitespaces.decode()

    # filter out profiling messages
    lines_filtered = []
    for line in out.split('\n'):
        if not line.startswith("profiling:"):
            lines_filtered.append(line)

    return '\n'.join(lines_filtered)
