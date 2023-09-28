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
const std::string alphaNumeric{"[a-z0-9]+"};

// separator defines the separators allowed to be embedded in name
// components. This allows one period, one or two underscore and multiple
// dashes. Repeated dashes and underscores are intentionally treated
// differently. In order to support valid hostnames as name components,
// supporting repeated dash was added. Additionally double underscore is
// now allowed as a separator to loosen the restriction for previously
// supported names.
const std::string separator{"(?:[._]|__|[-]+)"};

// pathComponent restricts registry path components to start
// with at least one letter or number, with following parts able to be
// separated by one period, one or two underscore and multiple dashes.
const std::string pathComponent = concatenate({ alphaNumeric,
                                                optional(repeated(separator + alphaNumeric))
                                              });

// domainNameComponent restricts the registry domain component of a
// repository name to start with a component as defined by DomainRegexp
const std::string domainNameComponent{"(?:[a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9-]*[a-zA-Z0-9])"};

// Only IPv6 in compressed or uncompressed format are allowed by this expression.
// Other formats like IPv6 zone identifiers or Special addresses are excluded.
// For general recommendations about IPv6 addresses text representations,
// refer to IETF RFC 5952.
const std::string ipv6Address{"\\[(?:[a-fA-F0-9:]+)\\]"};

const std::string port{"\\:[0-9]+"};

// domainName defines the structure of potential domain components
// that may be part of image names. This is purposely a subset of what is
// allowed by DNS to ensure backwards compatibility with Docker image
// names.
const std::string domainName = concatenate({ domainNameComponent,
                                             optional(repeated("\\." + domainNameComponent))
                                           });

// host defines the structure of potential domains based on the URI Host
// subcomponent on IETF RFC 3986.
const std::string host = group(concatenate({domainName, "|", ipv6Address}));

const std::string domain = host + optional(port);

// remoteName matches the remote-name of a repository. It consists of one
// or more forward slash (/) delimited path-components (i.e. <namespace>/<repo name>)
const std::string remoteName = concatenate ({ pathComponent,
                                              optional(repeated("\\/" + pathComponent))
                                            });

// name is the format for the name component of references.
const std::string name = concatenate({ optional(domain + "\\/"),
                                       remoteName
                                     });

// tag matches valid tag names. From docker/docker:graph/tags.go.
const std::string tag{"[\\w][\\w.-]{0,127}"};

// digest matches valid digests.
const std::string digest{"[A-Za-z][A-Za-z0-9]*(?:[-_+.][A-Za-z][A-Za-z0-9]*)*[:][0-9A-Fa-f]{32,}"};

// reference is the full supported format of a reference. The regexp
// is anchored and has capturing groups for name, tag, and digest
// components.
const std::string reference = anchored(capture(name)
                                       + optional("\\:" + capture(tag))
                                       + optional("\\@" + capture(digest))
                                      );

std::string concatenate(const std::initializer_list<std::string> expr) {
    auto output = std::stringstream{};
    for (const auto& exp : expr) {
        output << exp;
    }
    return output.str();
}

// Wraps the expression in a non-capturing group and makes the group optional.
std::string optional(const std::string& expr) {
    return group(expr) + "?";
}

// Wraps the regexp in a non-capturing group to get one or more matches.
std::string repeated(const std::string& expr) {
    return group(expr) + "+";
}

// Wraps the regexp in a non-capturing group.
std::string group(const std::string& expr) {
    return "(?:" + expr + ")";
}

// Wraps the expression in a capturing group.
std::string capture(const std::string& expr) {
    return "(" + expr + ")";
}

// Anchors the regular expression by adding start and end delimiters.
std::string anchored(const std::string& expr) {
    return "^" + expr + "$";
}

} //namespace

const boost::regex domain(strings::domain);
const boost::regex name(strings::name);
const boost::regex tag(strings::tag);
const boost::regex digest(strings::digest);
const boost::regex reference(strings::reference);

} // namespace
} // namespace
} // namespace
