/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "GroupDB.hpp"

#include <fstream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include "common/Error.hpp"

namespace sarus {
namespace common {

void GroupDB::read(const boost::filesystem::path& file) {
    std::ifstream is{file.c_str()};
    read(is);
}

void GroupDB::read(std::istream& is) {
    std::string line;
    while(std::getline(is, line)) {
        entries.push_back(parseLine(line));
    }
}

void GroupDB::write(const boost::filesystem::path& file) const {
    std::ofstream os{file.c_str()};
    for(const auto& entry : entries) {
        os  << entry.groupName << ":"
            << entry.encryptedPassword << ":"
            << entry.gid << ":"
            << boost::join(entry.users, ",")
            << "\n";
    }
}

const std::vector<GroupDB::Entry>& GroupDB::getEntries() const {
    return entries;
}

std::vector<GroupDB::Entry>& GroupDB::getEntries() {
    return entries;
}

GroupDB::Entry GroupDB::parseLine(const std::string& line) const {
    auto tokens = std::vector<std::string>{};
    boost::split(tokens, line, boost::is_any_of(":"));
    if(tokens.size() < 3 || tokens.size() > 4) {
        auto message = boost::format("Failed to parse line \"%s\": bad number of tokens") % line;
        SARUS_THROW_ERROR(message.str());
    }

    auto entry = Entry{};
    entry.groupName = tokens[0];
    entry.encryptedPassword = tokens[1];
    entry.gid = std::stoul(tokens[2]);

    if(tokens.size() > 3 && !tokens[3].empty()) {
        boost::split(entry.users, tokens[3], boost::is_any_of(","));
    }

    return entry;
}

}} // namespace
