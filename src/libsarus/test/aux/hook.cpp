/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hook.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>

#include <boost/format.hpp>

#include "libsarus/Error.hpp"


namespace libsarus {
namespace test {
namespace aux {
namespace hook {

void writeOCIContainerStateToStdin(const boost::filesystem::path& bundleDir) {
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

rapidjson::Document createOCIBaseConfigJSON(const boost::filesystem::path& rootfsDir, const std::tuple<uid_t, gid_t>& idsOfUser) {
    namespace rj = rapidjson;

    auto doc = rj::Document{rj::kObjectType};
    auto& allocator = doc.GetAllocator();

    // root
    doc.AddMember(
        "root",
        rj::Value{rj::kObjectType},
        allocator);
    doc["root"].AddMember(
        "path",
        rj::Value{rootfsDir.filename().c_str(), allocator},
        allocator);

    // process
    doc.AddMember(
        "process",
        rj::Document{rj::kObjectType},
        allocator);
    doc["process"].AddMember(
        "user",
        rj::Document{rj::kObjectType},
        allocator);
    doc["process"]["user"].AddMember(
        "uid",
        rj::Value{std::get<0>(idsOfUser)},
        allocator);
    doc["process"]["user"].AddMember(
        "gid",
        rj::Value{std::get<1>(idsOfUser)},
        allocator);
    doc["process"].AddMember(
        "env",
        rj::Document{rj::kArrayType},
        allocator);

    // annotations
    doc.AddMember(
        "annotations",
        rj::Document{rj::kObjectType},
        allocator);

    return doc;
}

}}}}
