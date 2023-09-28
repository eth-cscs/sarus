/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef cli_regex_hpp
#define cli_regex_hpp

#include <string>

#include <boost/regex.hpp>

namespace sarus {
namespace cli {
namespace regex {

extern const boost::regex domain;
extern const boost::regex name;
extern const boost::regex tag;
extern const boost::regex digest;
extern const boost::regex reference;

namespace strings {

extern const std::string alphaNumeric;
extern const std::string separator;
extern const std::string pathComponent;
extern const std::string domainNameComponent;
extern const std::string ipv6Address;
extern const std::string port;
extern const std::string domainName;
extern const std::string host;
extern const std::string domain;
extern const std::string remoteName;
extern const std::string namePattern;
extern const std::string name;
extern const std::string tag;
extern const std::string digest;
extern const std::string reference;

std::string concatenate(const std::initializer_list<std::string> expr);
std::string optional(const std::string& expr);
std::string repeated(const std::string& expr);
std::string group(const std::string& expr);
std::string capture(const std::string& expr);
std::string anchored(const std::string& expr);

}
}
}
}

#endif
