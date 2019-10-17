# Sarus

Copyright (c) 2018-2019, ETH Zurich. All rights reserved.

Please, refer to the LICENSE files in the `licenses` directory.
SPDX-License-Identifier: BSD-3-Clause

Basic instructions to get started with Sarus standalone follow below.
**NOTE:** Standalone installation is only available starting from version 1.0.0-rc8.

Please refer to the Sarus documentation for more information on how to install,
configure and use Sarus (https://sarus.readthedocs.io).

1. Get your standanlone `sarus.tar.gz` and extract it in an installation directory.

        sudo mkdir /opt/sarus
        cd /opt/sarus
        # download here your sarus.tar.gz from https://github.com/eth-cscs/sarus/releases
        sudo tar xf sarus.tar.gz

3. Run the configuration script to finalize the installation of Sarus.

        cd /opt/sarus/1.0.0-rc8 # adapt folder name to actual version of Sarus
        sudo ./configure_installation.sh

4. Use Sarus, e.g.:

        sarus images
        sarus run alpine cat /etc/os-release
