/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_OCIHook_hpp
#define sarus_runtime_OCIHook_hpp

#include <tuple>
#include <vector>
#include <string>
#include <memory>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"

namespace sarus {
namespace runtime {

class OCIHook {
public:
    class Condition {
    public:
        virtual ~Condition() = default;
        virtual bool evaluate(std::shared_ptr<const common::Config>) const = 0;
    };

    class ConditionAlways : public Condition {
    public:
        ConditionAlways(bool);
        bool evaluate(std::shared_ptr<const common::Config>) const override;
    private:
        bool value;
    };

    class ConditionAnnotations : public Condition {
    public:
        ConditionAnnotations(const std::vector<std::tuple<std::string, std::string>>& annotations);
        bool evaluate(std::shared_ptr<const common::Config>) const override;
    private:
        std::vector<std::tuple<std::string, std::string>> annotations;
    };

    class ConditionCommands : public Condition {
    public:
        ConditionCommands(const std::vector<std::string>& commands);
        bool evaluate(std::shared_ptr<const common::Config>) const override;
    private:
        std::vector<std::string> commands;
    };

    class ConditionHasBindMounts : public Condition {
    public:
        ConditionHasBindMounts(bool);
        bool evaluate(std::shared_ptr<const common::Config>) const override;
    private:
        bool value;
    };

public:
    bool isActive(std::shared_ptr<const common::Config>) const;

public:
    boost::filesystem::path jsonFile;
    std::string version;
    rapidjson::Document jsonHook; // OCI hook object as defined by the OCI Runtime Specification:
                                  // https://github.com/opencontainers/runtime-spec/blob/master/config.md#posix-platform-hooks
    std::vector<std::unique_ptr<Condition>> conditions;
    std::vector<std::string> stages;
};

}} // namespaces

#endif
