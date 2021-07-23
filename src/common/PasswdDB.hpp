/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_hooks_ssh_PasswdDB_hpp
#define sarus_hooks_ssh_PasswdDB_hpp

#include <string>
#include <vector>
#include <linux/types.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include "common/Logger.hpp"

namespace sarus {
namespace common {

class PasswdDB {
public:
    struct Entry {
        std::string loginName;
        std::string encryptedPassword;
        uid_t uid;
        gid_t gid;
        std::string userNameOrCommentField;
        boost::filesystem::path userHomeDirectory;
        boost::optional<boost::filesystem::path> userCommandInterpreter;
    };

public:
    PasswdDB() = default;
    PasswdDB(const boost::filesystem::path&);
    PasswdDB(std::istream&);
    void write(const boost::filesystem::path&) const;
    std::string getUsername(uid_t) const;
    boost::filesystem::path getHomeDirectory(uid_t uid) const;
    const std::vector<Entry>& getEntries() const;
    std::vector<Entry>& getEntries();

private:
    void read(std::istream&);
    Entry parseLine(const std::string& line) const;
    void logMessage(const boost::format&, common::LogLevel,
                    std::ostream& out=std::cout, std::ostream& err=std::cerr) const;
    void logMessage(const std::string&, common::LogLevel,
                    std::ostream& out=std::cout, std::ostream& err=std::cerr) const;

private:
    std::vector<Entry> entries;
};

}} // namespace

#endif
