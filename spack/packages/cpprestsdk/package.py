from spack import *


class Cpprestsdk(CMakePackage):
    """The C++ REST SDK is a Microsoft project for cloud-based client-server
       communication in native code using a modern asynchronous C++ API design.
       This project aims to help C++ developers connect to and interact with
       services. """

    homepage = "https://github.com/Microsoft/cpprestsdk"
    url      = "https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz"
    git      = "https://github.com/microsoft/cpprestsdk.git"

    version('2.10.0', tag='v2.10.0')
    version('2.9.1',  tag='v2.9.1')

    depends_on('boost@1.65.0 cxxstd=11')
    depends_on('openssl')

    root_cmakelists_dir = 'Release'

    def cmake_args(self):
        args = ['-DWERROR=False',]
        return args
