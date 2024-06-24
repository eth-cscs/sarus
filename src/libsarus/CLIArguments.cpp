/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "CLIArguments.hpp"

#include <iterator>
#include <sstream>
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "libsarus/Error.hpp"

namespace libsarus {

CLIArguments::CLIArguments() {
    args.push_back(nullptr); // array is null-terminated
}

CLIArguments::CLIArguments(const CLIArguments& rhs) : CLIArguments() {
    for(int i=0; i<rhs.argc(); ++i) {
        push_back(rhs.argv()[i]);
    }
}

CLIArguments::CLIArguments(int argc, char* argv[]) : CLIArguments() {
    for(int i=0; i<argc; ++i) {
        push_back(argv[i]);
    }
}

CLIArguments::CLIArguments(std::initializer_list<std::string> args) : CLIArguments() {
    for(const auto& arg : args) {
        push_back(arg);
    }
}

CLIArguments::~CLIArguments() {
    clear();
}

CLIArguments& CLIArguments::operator=(const CLIArguments& rhs) {
    clear();
    for(auto* ptr : rhs) {
        push_back(ptr);
    }
    return *this;
}

void CLIArguments::push_back(const std::string& arg) {
    args.pop_back();
    args.push_back(strdup(arg.c_str()));
    args.push_back(nullptr);
}

int CLIArguments::argc() const {
    return args.size() - 1;
}

char** CLIArguments::argv() const {
    return const_cast<char**>(args.data());
}

CLIArguments::const_iterator CLIArguments::begin() const {
    return args.cbegin();
}

CLIArguments::const_iterator CLIArguments::end() const {
    return args.cend() - 1;
}

CLIArguments& CLIArguments::operator+=(const CLIArguments& rhs) {
    for(auto* ptr : rhs) {
        push_back(ptr);
    }
    return *this;
}

bool CLIArguments::empty() const {
    return begin() == end();
}

void CLIArguments::clear() {
    for(auto* ptr : args) {
        free(ptr);
    }
    args = { nullptr };
}

std::string CLIArguments::string() const {
    auto stringArgs = std::vector<std::string>{this->begin(), this->end()};
    return boost::algorithm::join(stringArgs, " ");
}

bool operator==(const CLIArguments& lhs, const CLIArguments& rhs) {
    if(lhs.argc() != rhs.argc()) {
        return false;
    }

    for(int i=0; i<lhs.argc(); ++i) {
        auto lhsString = lhs.argv()[i];
        auto rhsString = rhs.argv()[i];
        if(strcmp(lhsString, rhsString) != 0) {
            return false;
        }
    }

    return true;
}

const CLIArguments operator+(const CLIArguments& lhs, const CLIArguments& rhs) {
    auto result = lhs;
    result += rhs;
    return result;
}

std::ostream& operator<<(std::ostream& os, const CLIArguments& args) {
    os << "[";
    bool isFirstArg = true;
    for(const auto& arg : args) {
        if(!isFirstArg) {
            os << ", ";
        }
        else {
            isFirstArg = false;
        }
        os << "\"" << arg << "\"";
    }
    os << "]";
    return os;
}

std::istream& operator>>(std::istream& is, CLIArguments& args) {
    rapidjson::IStreamWrapper sw(is);
    rapidjson::Document doc;
    doc.ParseStream(sw);

    if(!doc.IsArray()) {
        SARUS_THROW_ERROR("Failed to deserialize CLIArguments from JSON"
                            " input stream. Expected a JSON array.");
    }

    args.clear();
    for(const auto& arg : doc.GetArray()) {
        args.push_back(arg.GetString());
    }

    return is;
}

}
