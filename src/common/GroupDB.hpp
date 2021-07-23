/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_GroupDB_hpp
#define sarus_common_GroupDB_hpp

#include <string>
#include <vector>
#include <linux/types.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

namespace sarus {
namespace common {

class GroupDB {
public:
    struct Entry {
        std::string groupName;
        std::string encryptedPassword;
        gid_t gid;
        std::vector<std::string> users;
    };

public:
    void read(const boost::filesystem::path&);
    void read(std::istream&);
    void write(const boost::filesystem::path&) const;
    const std::vector<Entry>& getEntries() const;
    std::vector<Entry>& getEntries();

private:
    Entry parseLine(const std::string& line) const;

private:
    std::vector<Entry> entries;
};

}} // namespace

#endif
