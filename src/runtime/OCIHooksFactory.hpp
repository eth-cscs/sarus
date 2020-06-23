/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_OCIHooksFactory_hpp
#define sarus_runtime_OCIHooksFactory_hpp

#include <memory>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "OCIHook.hpp"

namespace sarus {
namespace runtime {

class OCIHooksFactory {
public:
    std::vector<OCIHook> createHooks(const boost::filesystem::path& hooksDir,
                                     const boost::filesystem::path& schemaFile) const;
    OCIHook createHook(const boost::filesystem::path& jsonFile,
                       const boost::filesystem::path& schemaFile) const;
    std::unique_ptr<OCIHook::Condition> createCondition(const std::string& name,
                                                        const rapidjson::Value& value) const;
};

}} // namespaces

#endif
