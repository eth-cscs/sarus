/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_OCIBundleConfig_hpp
#define sarus_runtime_OCIBundleConfig_hpp

#include <memory>
#include <linux/types.h>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>

#include "common/Config.hpp"
#include "runtime/ConfigsMerger.hpp"


namespace sarus {
namespace runtime {

class OCIBundleConfig {
public:
    OCIBundleConfig(std::shared_ptr<const common::Config>);
    void generateConfigFile() const;
    const boost::filesystem::path& getConfigFile() const;

private:
    void makeJsonDocument() const;
    rapidjson::Value makeMemberProcess() const;
    rapidjson::Value makeMemberRoot() const;
    rapidjson::Value makeMemberMounts() const;
    rapidjson::Value makeMemberLinux() const;

    boost::optional<gid_t> findGidOfTtyGroup() const;

private:
    std::shared_ptr<const common::Config> config;
    ConfigsMerger configsMerger;
    std::shared_ptr<rapidjson::Document> document;
    rapidjson::MemoryPoolAllocator<>* allocator;
    boost::filesystem::path configFile;
};

}
}

#endif
