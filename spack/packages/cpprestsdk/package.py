from spack import *


class Cpprestsdk(CMakePackage):
    """The C++ REST SDK is a Microsoft project for cloud-based client-server
       communication in native code using a modern asynchronous C++ API design.
       This project aims to help C++ developers connect to and interact with
       services. """

    homepage = "https://github.com/Microsoft/cpprestsdk"
    url      = "https://github.com/Microsoft/cpprestsdk/archive/v2.10.0.tar.gz"

    version('2.10.0', '49b5b7789fa844df0e78cc7591aed095')
    version('2.9.1',  'c3dd67d8cde8a65c2e994e2ede4439a2')

    depends_on('boost@1.65.0')
    depends_on('openssl')

    root_cmakelists_dir = 'Release'

    def cmake_args(self):
        args = ['-DWERROR=False',]
        return args
