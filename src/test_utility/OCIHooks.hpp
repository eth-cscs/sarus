#ifndef sarus_test_utility_OCIHooks_hpp
#define sarus_test_utility_OCIHooks_hpp

#include <cstdio>
#include <iostream>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "common/Error.hpp"


namespace test_utility {
namespace ocihooks {

void writeContainerStateToStdin(const boost::filesystem::path& bundleDir) {
    auto state = boost::format{
        "{"
            "\"ociVersion\": \"dummy-version\", "
            "\"id\": \"container-mpi-hook-test\", "
            "\"status\": \"running\", "
            "\"pid\": %d, "
            "\"bundle\": %s"
        "}"
    } % getpid() % bundleDir;

    boost::filesystem::path simulatedStdin = bundleDir / "simulated_stdin.txt";

    {
        std::ofstream os(simulatedStdin.c_str());
        os << state << std::endl;
    }

    if(std::freopen(simulatedStdin.c_str(), "r", stdin) == nullptr) {
        SARUS_THROW_ERROR("Failed to replace stdin with text file");
    }
    // Replacing stdin through std::freopen might result in a std::cin's error state when the
    // reader reaches the end of the stream. Let's clear the error state, in case that an error
    // was generated in a previous test.
    std::cin.clear();
}

}
}

#endif
