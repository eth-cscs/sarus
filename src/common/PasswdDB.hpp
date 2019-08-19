/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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
    void read(const boost::filesystem::path&);
    void write(const boost::filesystem::path&) const;
    const std::vector<Entry>& getEntries() const;
    std::vector<Entry>& getEntries();

private:
    Entry parseLine(const std::string& line) const;
    std::vector<std::string> splitLine(const std::string& line) const;

private:
    std::vector<Entry> entries;
};

}} // namespace

#endif
