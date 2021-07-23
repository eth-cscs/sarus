# Sarus

Copyright (c) 2018-2021, ETH Zurich. All rights reserved.

Please, refer to the LICENSE files in the `licenses` directory.
SPDX-License-Identifier: BSD-3-Clause

To get started with Sarus, follow these simple steps.

1. Get the standalone archive from https://github.com/eth-cscs/sarus/releases and extract it in your installation directory:

        sudo mkdir /opt/sarus && cd /opt/sarus
        # Pick your preferred version, this example is for Sarus @SARUS_VERSION@
        sudo wget https://github.com/eth-cscs/sarus/releases/download/@SARUS_VERSION@/sarus-Release.tar.gz
        sudo tar xfv sarus-Release.tar.gz

2. Run the configuration script to finalize the installation of Sarus:

        # adapt folder name to actual version of Sarus
        cd /opt/sarus/@SARUS_VERSION@-Release
        sudo ./configure_installation.sh
        # Follow the printed instructions to add Sarus to your PATH

3. Try Sarus:

        sarus version
        sarus pull alpine
        sarus run alpine cat /etc/os-release

For more information on how to install, configure and use Sarus,
please refer to the official documentation at https://sarus.readthedocs.io.
