/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "PasswdDB.hpp"

#include <fstream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include "libsarus/Error.hpp"

namespace libsarus {

PasswdDB::PasswdDB(const boost::filesystem::path& file) {
    std::ifstream is{file.c_str()};
    read(is);
}

PasswdDB::PasswdDB(std::istream& is) {
    read(is);
}

void PasswdDB::write(const boost::filesystem::path& file) const {
    std::ofstream os{file.c_str()};
    for(const auto& entry : entries) {
        os  << entry.loginName
            << ":" << entry.encryptedPassword
            << ":" << entry.uid
            << ":" << entry.gid
            << ":" << entry.userNameOrCommentField
            << ":" << entry.userHomeDirectory.string()
            << ":";

        if(entry.userCommandInterpreter) {
            os << entry.userCommandInterpreter->string();
        }

        os << "\n";
    }
}

std::string PasswdDB::getUsername(uid_t uid) const {
    // Note: the retrieval of a username is usually done through the getpwuid function.
    // However, musl fails to retrieve all the passwd entries from certain systems, e.g. LDAP at CSCS.

    for(const auto& entry : entries) {
        if(entry.uid == uid) {
            return entry.loginName;
        }
    }

    auto message = boost::format("Failed to retrieve username for uid=%d") % uid;
    SARUS_THROW_ERROR(message.str());
}

boost::filesystem::path PasswdDB::getHomeDirectory(uid_t uid) const {
    for(const auto& entry : entries) {
        if(entry.uid == uid) {
            logMessage(boost::format("Found home directory for uid=%d: %s") % uid % entry.userHomeDirectory,
                libsarus::LogLevel::DEBUG);
            return entry.userHomeDirectory;
        }
    }

    auto message = boost::format("Failed to retrieve home directory for uid=%d") % uid;
    SARUS_THROW_ERROR(message.str());
}

const std::vector<PasswdDB::Entry>& PasswdDB::getEntries() const {
    return entries;
}

std::vector<PasswdDB::Entry>& PasswdDB::getEntries() {
    return entries;
}

void PasswdDB::read(std::istream& is) {
    std::string line;
    while(std::getline(is, line)) {
        entries.push_back(parseLine(line));
    }
}

PasswdDB::Entry PasswdDB::parseLine(const std::string& line) const {
    auto tokens = std::vector<std::string>{};
    boost::split(tokens, line, boost::is_any_of(":"));
    if(tokens.size() < 6 || tokens.size() > 7) {
        auto message = boost::format("Failed to parse line \"%s\": bad number of tokens") % line;
        SARUS_THROW_ERROR(message.str());
    }

    auto entry = Entry{};
    entry.loginName = tokens[0];
    entry.encryptedPassword = tokens[1];
    entry.uid = std::stoul(tokens[2]);
    entry.gid = std::stoul(tokens[3]);
    entry.userNameOrCommentField = tokens[4];
    entry.userHomeDirectory = tokens[5];
    if(tokens.size() > 6 && !tokens[6].empty()) {
        entry.userCommandInterpreter = tokens[6];
    }

    return entry;
}

void PasswdDB::logMessage(const boost::format& message, libsarus::LogLevel level,
                std::ostream& out, std::ostream& err) const {
    logMessage(message.str(), level, out, err);
}

void PasswdDB::logMessage(const std::string& message, libsarus::LogLevel level,
                std::ostream& out, std::ostream& err) const {
    auto subsystemName = "PasswdDB";
    libsarus::Logger::getInstance().log(message, subsystemName, level, out, err);
}

}
