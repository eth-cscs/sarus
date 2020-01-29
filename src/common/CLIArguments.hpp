/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_common_CLIArguments_hpp
#define sarus_common_CLIArguments_hpp

#include <initializer_list>
#include <vector>
#include <string>
#include <cstring>
#include <ostream>
#include <istream>

namespace sarus {
namespace common {

/**
 * Utility class that wraps and manages the lifetime of the CLI arguments
 * to be passed to program (the char* argv[] parameter)
 */
class CLIArguments {
public:
    using const_iterator = typename std::vector<char*>::const_iterator;

public:
    CLIArguments();
    CLIArguments(const CLIArguments& rhs);
    CLIArguments(int argc, char* argv[]);
    CLIArguments(std::initializer_list<std::string> args);
    ~CLIArguments();

    CLIArguments& operator=(const CLIArguments& rhs);
    void push_back(const std::string& arg);

    int argc() const;
    char** argv() const;

    const_iterator begin() const;
    const_iterator end() const;

    CLIArguments& operator+=(const CLIArguments& rhs);

    bool empty() const;
    void clear();

private:
    std::vector<char*> args;
};

bool operator==(const CLIArguments&, const CLIArguments&);
const CLIArguments operator+(const CLIArguments&, const CLIArguments&);
std::ostream& operator<<(std::ostream&, const CLIArguments&);
std::istream& operator>>(std::istream&, CLIArguments&);

} // namespace
} // namespace

#endif
