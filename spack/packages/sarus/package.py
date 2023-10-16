# Copyright (c) 2018-2023 ETH Zurich. All rights reserved.
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
    url      = "https://github.com/eth-cscs/sarus/archive/1.6.1.tar.gz"
    git      = "https://github.com/eth-cscs/sarus.git"

    version('develop', branch='develop', submodules=True)
    version('master',  branch='master', submodules=True)
    version("1.6.1", tag="1.6.1", submodules=True)
    version("1.6.0", commit="9c01d76736940feb360175c515e5778e408e631e")
    version("1.5.2", commit="75e223bfe555c15f41d6394b41750c74d56e1d98")
    version("1.5.1", commit="9808a4ab7f86359d77f574962f020839266b9ab8")
    version("1.5.0", commit="3e244ec8db85978aa42b1c5f47bc94846958bd96")
    version("1.4.2", commit="13af2723b5c1cd1890d056af715946596f7ff2c6")
    version("1.4.1", commit="a73f6ca9cafb768f3132cfcef8c826af34eeff94")
    version("1.4.0", commit="c6190faf45d5e0ff5348c70c2d4b1e49b2e01039")
    version("1.3.3", commit="f2c000caf3d6a89ea019c70e2703da46799b0e9c")
    version("1.3.2", commit="ac6a1b8708ec402bbe810812d8af41d1b7bf1860")
    version("1.3.1", commit="5117a0da8d2171c4bf9ebc6835e0dd6b73812930")
    version("1.3.0", commit="f52686fa942d5fc2b1302011e9a081865285357b")
    version("1.2.0", commit="16d27c0c10366dcaa0c72c6ec72331b6e4e6884d")
    version("1.1.0", commit="ed5b640a45ced6f6a7a2a9d295d3d6c6106f39c3")
    version("1.0.1", commit="abb8c314a196207204826f7b60e5064677687405")
    version("1.0.0", commit="d913b1d0ef3729f9f41ac5bd06dd5615c407ced4")

    variant('ssh', default=True,
            description='Build and install the SSH hook and custom SSH software '
                        'to enable connections inside containers')
    variant('configure_installation', default=True,
            description='Run the script to setup a starting Sarus configuration as '
                        'part of the installation phase. Running the script requires '
                        'super-user privileges.')
    variant('unit_tests', default=False,
            description='Build unit test executables in the build directory. '
                        'Also downloads and builds internally the CppUTest framework.')

    depends_on('wget', type='build')
    depends_on('expat', when='@:1.4.2')
    depends_on('zlib', when='+ssh')
    depends_on('squashfs', type=('build', 'run'))
    depends_on('boost@1.65.0: cxxstd=11 +program_options +regex +filesystem')
    depends_on('cpprestsdk@2.10.0:', when='@:1.4.2')
    depends_on('libarchive@3.4.1:', when='@:1.4.2')
    depends_on('rapidjson@00dbcf2', type='build', when='@:1.4.2')
    depends_on("runc@:1.0.3", type=('build', 'run'))
    depends_on("tini", type=('build', 'run'))
    depends_on("skopeo", type=('build', 'run'), when='@1.5.0:')
    depends_on("umoci", type=('build', 'run'), when='@1.5.0:')

    # autoconf is required to build Dropbear for the SSH hook
    depends_on('autoconf', type='build')

    # Python 3 is used to run integration tests
    depends_on('python@3:', type='run', when='@develop')

    def cmake_args(self):
        spec = self.spec
        args = ['-DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain_files/gcc.cmake',
                '-DENABLE_SSH=%s' % ('+ssh' in spec),
                '-DENABLE_UNIT_TESTS=%s' % ('+unit_tests' in spec)]
        return args

    def install(self, spec, prefix):
        with working_dir(self.build_directory):
            make(*self.install_targets)
            mkdirp(prefix.var.OCIBundleDir)
            if '+configure_installation' in spec:
                self.configure_installation(spec, prefix)

    def configure_installation(selfself, spec, prefix):
        import subprocess
        script_path = prefix + '/configure_installation.sh'
        subprocess.check_call(script_path)
