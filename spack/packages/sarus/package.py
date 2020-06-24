# Copyright (c) 2018-2020 ETH Zurich. All rights reserved.
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

from spack import *


class Sarus(CMakePackage):
    """Sarus is an OCI-compliant container engine for HPC systems."""

    homepage = "https://github.com/eth-cscs/sarus"
    url      = "https://github.com/eth-cscs/sarus/archive/1.1.0.tar.gz"
    git      = "https://github.com/eth-cscs/sarus.git"

    version('develop', branch='develop')
    version('master',  branch='master')
    version('1.1.0',   tag='1.1.0')
    version('1.0.1',   tag='1.0.1')
    version('1.0.0',   tag='1.0.0')

    variant('ssh', default=True,
            description='Build and install the SSH hook and custom SSH software '
                        'to enable connections inside containers')
    variant('configure_installation', default=True,
            description='Run the script to setup a starting Sarus configuration as '
                        'part of the installation phase. Running the script requires '
                        'super-user privileges.')

    depends_on('squashfs', type=('build', 'run'))
    depends_on('boost@1.65.0 cxxstd=11')
    depends_on('cpprestsdk@2.10.0')
    depends_on('libarchive@3.4.1')
    depends_on('rapidjson@663f076', type='build')

    # Python 3 is used to run integration tests
    depends_on('python@3:', type='run', when='@develop')

    def cmake_args(self):
        spec = self.spec
        args = ['-DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain_files/gcc.cmake',
                '-DENABLE_SSH=%s' % ('+ssh' in spec)]
        return args

    def install(self, spec, prefix):
        with working_dir(self.build_directory):
            make(*self.install_targets)
            mkdirp(prefix.var.OCIBundleDir)
            self.install_runc(spec, prefix)
            self.install_tini(spec, prefix)
            if '+configure_installation' in spec:
                self.configure_installation(spec, prefix)

    def install_runc(self, spec, prefix):
        wget = which('wget')
        runc_url = 'https://github.com/opencontainers/runc/releases/download/v1.0.0-rc10/runc.amd64'
        runc_install_path = prefix.bin + '/runc.amd64'
        wget('-O', runc_install_path, runc_url)
        set_executable(runc_install_path)

    def install_tini(self, spec, prefix):
        wget = which('wget')
        tini_url = 'https://github.com/krallin/tini/releases/download/v0.18.0/tini-static-amd64'
        tini_install_path = prefix.bin + '/tini-static-amd64'
        wget('-O', tini_install_path, tini_url)
        set_executable(tini_install_path)

    def configure_installation(selfself, spec, prefix):
        import subprocess
        script_path = prefix + '/configure_installation.sh'
        subprocess.check_call(script_path)
