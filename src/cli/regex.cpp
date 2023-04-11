/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "cli/regex.hpp"

#include <sstream>


namespace sarus {
namespace cli {
namespace regex {
namespace strings {

// alphaNumeric defines the alpha numeric atom, typically a
// component of names. This only allows lower case characters and digits.
std::string alphaNumeric{"[a-z0-9]+"};

// separator defines the separators allowed to be embedded in name
// components. This allow one period, one or two underscore and multiple
// dashes. Repeated dashes and underscores are intentionally treated
// differently. In order to support valid hostnames as name components,
// supporting repeated dash was added. Additionally double underscore is
// now allowed as a separator to loosen the restriction for previously
// supported names.
std::string separator{"(?:[._]|__|[-]*)"};

// nameComponent restricts registry path component names to start
// with at least one letter or number, with following parts able to be
// separated by one period, one or two underscore and multiple dashes.
std::string nameComponent = concatenate({ alphaNumeric,
                                          optional(repeated(separator + alphaNumeric))
                                        });

// domainComponent restricts the registry domain component of a
// repository name to start with a component as defined by DomainRegexp
// and followed by an optional port.
std::string domainComponent{"(?:[a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9-]*[a-zA-Z0-9])"};

// domain defines the structure of potential domain components
// that may be part of image names. This is purposely a subset of what is
// allowed by DNS to ensure backwards compatibility with Docker image
// names.
std::string domain = concatenate({ domainComponent,
                                   optional(repeated("\\." + domainComponent)),
                                   optional("\\:" + std::string{"[0-9]+"})
                                 });

// name is the format for the name component of references. The
// regexp has capturing groups for the domain and name part omitting
// the separating forward slash from either.
std::string name = concatenate({ optional(domain + "\\/"),
                                 nameComponent,
                                 optional(repeated("\\/" + nameComponent))
                               });

// tag matches valid tag names. From docker/docker:graph/tags.go.
std::string tag{"[\\w][\\w.-]{0,127}"};

// digest matches valid digests.
std::string digest{"[A-Za-z][A-Za-z0-9]*(?:[-_+.][A-Za-z][A-Za-z0-9]*)*[:][0-9A-Fa-f]{32,}"};

// reference is the full supported format of a reference. The regexp
// is anchored and has capturing groups for name, tag, and digest
// components.
std::string reference = anchored(capture(name)
                                 + optional("\\:" + capture(tag))
                                 + optional("\\@" + capture(digest))
                                );

std::string concatenate(std::initializer_list<std::string> expr) {
    auto output = std::stringstream{};
    for (const auto& exp : expr) {
        output << exp;
    }
    return output.str();
}

// Wraps the expression in a non-capturing group and makes the group optional.
std::string optional(std::string expr) {
    return group(expr) + "?";
}

// Wraps the regexp in a non-capturing group to get one or more matches.
std::string repeated(std::string expr) {
    return group(expr) + "+";
}

// Wraps the regexp in a non-capturing group.
std::string group(std::string expr) {
    return "(?:" + expr + ")";
}

// Wraps the expression in a capturing group.
std::string capture(std::string expr) {
    return "(" + expr + ")";
}

// Anchors the regular expression by adding start and end delimiters.
std::string anchored(std::string expr) {
    return "^" + expr + "$";
}

} //namespace

boost::regex domain(strings::domain);
boost::regex name(strings::name);
boost::regex tag(strings::tag);
boost::regex digest(strings::digest);
boost::regex reference(strings::reference);

} // namespace
} // namespace
} // namespace
