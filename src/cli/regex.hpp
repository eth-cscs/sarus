/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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

extern boost::regex domain;
extern boost::regex name;
extern boost::regex tag;
extern boost::regex digest;
extern boost::regex reference;

namespace strings {

extern std::string alphaNumeric;
extern std::string separator;
extern std::string nameComponent;
extern std::string domainComponent;
extern std::string domain;
extern std::string name;
extern std::string tag;
extern std::string digest;
extern std::string reference;

extern std::string concatenate(std::initializer_list<std::string> expr);
extern std::string optional(std::string expr);
extern std::string repeated(std::string expr);
extern std::string group(std::string expr);
extern std::string capture(std::string expr);
extern std::string anchored(std::string expr);

}
}
}
}

#endif
