/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef libsarus_CLIArguments_hpp
#define libsarus_CLIArguments_hpp

#include <initializer_list>
#include <vector>
#include <string>
#include <cstring>
#include <ostream>
#include <istream>

namespace libsarus {

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
    template<class InputIter>
    CLIArguments(InputIter begin, InputIter end) : CLIArguments() {
        for(InputIter arg=begin; arg!=end; ++arg) {
            push_back(*arg);
        }
    };

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
    std::string string() const;

private:
    std::vector<char*> args;
};

bool operator==(const CLIArguments&, const CLIArguments&);
const CLIArguments operator+(const CLIArguments&, const CLIArguments&);
std::ostream& operator<<(std::ostream&, const CLIArguments&);
std::istream& operator>>(std::istream&, CLIArguments&);

}

#endif
