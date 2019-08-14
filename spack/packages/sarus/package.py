# Copyright (c) 2018-2019 ETH Zurich. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#     2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#     3. Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from this
#     software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import inspect
import subprocess
from spack import *


class Sarus(CMakePackage):
    """Sarus is an OCI-compliant container runtime for HPC systems.-"""

    homepage = "https://github.com/eth-cscs/sarus"
    url      = "https://github.com/eth-cscs/sarus/archive/1.0.0-rc6.tar.gz"
    git      = "https://github.com/eth-cscs/sarus.git"

    version('develop', branch='master')
    version('1.0.0-rc6', '5c803adf10e1f10d34e83c411b07cbb8')
    version('1.0.0-rc5', 'd894a96fa350af8947f06e7e8c73d59f')

    variant('ssh', default=True,
            description='Build and install the SSH hook and custom OpenSSH software '
                        'to enable connections inside containers')
    variant('runtime_security_checks', default=True,
            description='Enable runtime security checks (root ownership of files, etc.). '
                        'Strongly recommended for production deployments. '
                        'Disable to simplify setup of test installations.')

    depends_on('squashfs', type=('build', 'run'))
    depends_on('boost@1.65.0')
    depends_on('cpprestsdk@2.10.0')
    depends_on('libarchive@3.3.1')
    depends_on('rapidjson@663f076')

    depends_on('python@2.7.15', type='build', when='@develop')

    def cmake_args(self):
        spec = self.spec
        args = ['-DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain_files/gcc-nowerror.cmake',
                '-DENABLE_SSH=%s' % ('+ssh' in spec),
                '-DENABLE_RUNTIME_SECURITY_CHECKS=%s' % ('+runtime_security_checks' in spec)]
        return args

    def install(self, spec, prefix):
        with working_dir(self.build_directory):
            make(*self.install_targets)
            mkdirp(prefix.var.sarus.OCIBundleDir)

    @run_after('install')
    @on_package_attributes(run_tests=True)
    def test_installation(self):
        # Check image pull
        command = ["sarus", "pull", "alpine"]
        subprocess.check_call(command)

        # Check container run
        command = ["sarus", "run", "alpine", "cat", "/etc/os-release"]
        output = subprocess.check_call(command)
        lines = [line for line in output.split('\n')]
        assert 'NAME="Alpine Linux"' in lines[0]
