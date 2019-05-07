/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "HelpMessage.hpp"


namespace sarus {
namespace cli {

HelpMessage::HelpMessage()
    : optionsDescription{new boost::program_options::options_description}
{}

HelpMessage::HelpMessage(const HelpMessage& rhs)
    : usage{rhs.usage}
    , description{rhs.description}
    , optionsDescription{new boost::program_options::options_description{*rhs.optionsDescription}}
{}


HelpMessage& HelpMessage::setUsage(const std::string& usage) {
    this->usage = usage;
    return *this;
}

HelpMessage& HelpMessage::setDescription(const std::string& description) {
    this->description = description;
    return *this;
}

HelpMessage& HelpMessage::setOptionsDescription(const boost::program_options::options_description& optionsDescription) {
    // create a dynamically allocated copy of the "options description"
    // (we cannot simply store it by-value because options_description has no copy assignment operator)
    auto copy = new boost::program_options::options_description{optionsDescription};
    this->optionsDescription.reset(copy);
    return *this;
}

std::ostream& operator<<(std::ostream& os, const HelpMessage& printer) {
    os  << "Usage: " << printer.usage << "\n"
        << "\n"
        << printer.description << "\n"
        << "\n"
        << *printer.optionsDescription;
    return os;
}

}
}
