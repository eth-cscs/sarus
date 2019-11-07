/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "GroupDB.hpp"

#include <fstream>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Utility.hpp"

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
            << entry.gid << ":";

        if(!entry.users.empty()) {
            os << entry.users.front();

            for(auto user = entry.users.begin()+1; user != entry.users.end(); ++user) {
                os << "," << *user;
            }
        }

        os << "\n";
    }
}

const std::vector<GroupDB::Entry>& GroupDB::getEntries() const {
    return entries;
}

std::vector<GroupDB::Entry>& GroupDB::getEntries() {
    return entries;
}

GroupDB::Entry GroupDB::parseLine(const std::string& line) const {
    auto tokens = splitLine(line);
    if(tokens.size() < 3 || tokens.size() > 4) {
        auto message = boost::format("Failed to parse line \"%s\": bad number of tokens") % line;
        SARUS_THROW_ERROR(message.str());
    }

    auto entry = Entry{};
    entry.groupName = tokens[0];
    entry.encryptedPassword = tokens[1];
    entry.gid = std::stoul(tokens[2]);

    if(tokens.size() > 3) {
        entry.users = common::convertStringListToVector<std::string>(tokens[3], ',');
    }

    return entry;
}

//TODO: replace this function with common::convertStringListToVector
std::vector<std::string> GroupDB::splitLine(const std::string& line) const {
    auto tokens = std::vector<std::string>{};

    auto tokenBeg = line.cbegin();
    while(tokenBeg != line.cend()) {
        auto tokenEnd = std::find(tokenBeg, line.cend(), ':');
        tokens.emplace_back(tokenBeg, tokenEnd);
        tokenBeg = tokenEnd != line.cend() ? tokenEnd + 1 : tokenEnd;
    }

    return tokens;
}

}} // namespace
