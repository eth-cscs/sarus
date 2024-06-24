/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_glibc_GlibcHook_hpp
#define sarus_hooks_glibc_GlibcHook_hpp

#include <vector>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <sys/types.h>

#include "libsarus/Logger.hpp"
#include "libsarus/UserIdentity.hpp"

namespace sarus {
namespace hooks {
namespace glibc {

class GlibcHook {
public:
    GlibcHook();
    void injectGlibcLibrariesIfNecessary();

private:
    void parseConfigJSONOfBundle();
    void parseEnvironmentVariables();
    bool containerHasGlibc() const;
    std::vector<boost::filesystem::path> get64bitContainerLibraries() const;
    boost::optional<boost::filesystem::path> findLibc(const std::vector<boost::filesystem::path>& libs) const;
    bool containerGlibcHasToBeReplaced() const;
    std::tuple<unsigned int, unsigned int> detectHostLibcVersion() const;
    std::tuple<unsigned int, unsigned int> detectContainerLibcVersion() const;
    void verifyThatHostAndContainerGlibcAreABICompatible(
        const boost::filesystem::path& hostLibc,
        const boost::filesystem::path& containerLibc) const;
    void replaceGlibcLibrariesInContainer() const;
    void logMessage(const std::string& message, libsarus::LogLevel logLevel,
                    std::ostream& out=std::cout, std::ostream& err=std::cerr) const;
    void logMessage(const boost::format& message, libsarus::LogLevel,
                    std::ostream& out=std::cout, std::ostream& err=std::cerr) const;

private:
    boost::filesystem::path bundleDir;
    boost::filesystem::path rootfsDir;
    pid_t pidOfContainer;
    libsarus::UserIdentity userIdentity;
    boost::filesystem::path lddPath;
    boost::filesystem::path ldconfigPath;
    boost::filesystem::path readelfPath;
    std::vector<boost::filesystem::path> hostLibraries;
    std::vector<boost::filesystem::path> containerLibraries;
};

}}} // namespace

#endif
