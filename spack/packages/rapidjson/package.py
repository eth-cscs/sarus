from spack import *


class Rapidjson(CMakePackage):
    """A fast JSON parser/generator for C++ with both SAX/DOM style API"""

    homepage = "http://rapidjson.org"
    url      = "https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz"
    git      = 'https://github.com/Tencent/rapidjson.git',

    version('00dbcf2',
            git = 'https://github.com/Tencent/rapidjson.git',
            commit = '00dbcf2')

    # Suppress compiler warning about string overflow
    # This warning has been observed only on Fedora 34 (could be due to a more recent compiler)
    # For reference see https://github.com/Tencent/rapidjson/issues/1924#issuecomment-907073451
    def cmake_args(self):
        args = ['-DCMAKE_CXX_FLAGS=-Wno-error=stringop-overflow']
        return args
