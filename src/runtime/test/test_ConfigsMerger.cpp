/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "common/ImageMetadata.hpp"
#include "runtime/ConfigsMerger.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace runtime {
namespace test {

TEST_GROUP(ConfigsMergerTestGroup) {
};

TEST(ConfigsMergerTestGroup, workdir) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto metadata = common::ImageMetadata{};

    // workdir by default
    CHECK((ConfigsMerger{config, metadata}.getWorkdirInContainer() == "/"));

    // workdir from metadata
    metadata.workdir = "/workdir-from-metadata";
    CHECK((ConfigsMerger{config, metadata}.getWorkdirInContainer() == "/workdir-from-metadata"));

    // workdir from CLI option
    config->commandRun.workdir = "/workdir-from-cli";
    CHECK((ConfigsMerger{config, metadata}.getWorkdirInContainer() == "/workdir-from-cli"));
}

TEST(ConfigsMergerTestGroup, environment) {
    namespace rj = rapidjson;
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto& allocator = config->json.GetAllocator();
    auto metadata = common::ImageMetadata{};

    // empty environment
    {
        config->commandRun.hostEnvironment = {};
        metadata.env = {};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer().empty()));
    }
    // no image metadata variables
    {
        config->commandRun.hostEnvironment = {{"KEY", "HOST_VALUE"}};
        metadata.env = {};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"KEY", "HOST_VALUE"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // no host variables
    {
        config->commandRun.hostEnvironment = {};
        metadata.env = {{"KEY", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"KEY", "IMAGE_VALUE"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // image metadata environment overrides host environment
    {
        config->commandRun.hostEnvironment = {{"KEY", "HOST_VALUE"}};
        metadata.env = {{"KEY", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"KEY", "IMAGE_VALUE"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // config file set environment overrides both host and image metadata
    {
        rj::Value environmentValue(rj::kObjectType);
        rj::Value setValue(rj::kObjectType);
        setValue.AddMember("SARUS_CONFIG_SET", "config_set_value", allocator);
        environmentValue.AddMember("set", setValue, allocator);
        config->json.AddMember("environment", environmentValue, allocator);

        config->commandRun.hostEnvironment = {{"SARUS_CONFIG_SET", "HOST_VALUE"}};
        metadata.env = {{"SARUS_CONFIG_SET", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"SARUS_CONFIG_SET", "config_set_value"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));

        config->json["environment"].RemoveMember("set");
    }
    // config file prepend/append
    {
        rj::Value prependValue(rj::kObjectType);
        prependValue.AddMember("SARUS_CONFIG_PREPEND_APPEND", "config_prepend_value", allocator);
        rj::Value appendValue(rj::kObjectType);
        appendValue.AddMember("SARUS_CONFIG_PREPEND_APPEND", "config_append_value", allocator);
        config->json["environment"].AddMember("prepend", prependValue, allocator);
        config->json["environment"].AddMember("append", appendValue, allocator);

        config->commandRun.hostEnvironment = {};
        metadata.env = {{"SARUS_CONFIG_PREPEND_APPEND", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{
            {"SARUS_CONFIG_PREPEND_APPEND", "config_prepend_value:IMAGE_VALUE:config_append_value"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));

        config->json["environment"].RemoveMember("prepend");
        config->json["environment"].RemoveMember("append");
    }
    // config file unset
    {
        rj::Value unsetValue(rj::kArrayType);
        unsetValue.PushBack("SARUS_CONFIG_UNSET", allocator);
        config->json["environment"].AddMember("unset", unsetValue, allocator);

        config->commandRun.hostEnvironment = {};
        metadata.env = {{"SARUS_CONFIG_UNSET", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));

        config->json["environment"].RemoveMember("unset");
    }
    // cli sets variable
    {
        config->commandRun.hostEnvironment = {};
        config->commandRun.userEnvironment = {{"CLI_VAR", "cli_value"}};
        metadata.env = {};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"CLI_VAR", "cli_value"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // cli overrides all
    {
        rj::Value setValue(rj::kObjectType);
        setValue.AddMember("SARUS_CONFIG_SET", "config_set_value", allocator);
        config->json["environment"].AddMember("set", setValue, allocator);

        config->commandRun.hostEnvironment = {{"SARUS_CONFIG_SET", "HOST_VALUE"}};
        config->commandRun.userEnvironment = {{"SARUS_CONFIG_SET", "cli_value"}};
        metadata.env = {{"SARUS_CONFIG_SET", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"SARUS_CONFIG_SET", "cli_value"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));
    }
    // cli empties values (Docker --env has this behavior)
    // reuses config->json["environment"]["set"] values from previous test
    {
        config->commandRun.hostEnvironment = {};
        config->commandRun.userEnvironment = {{"SARUS_CONFIG_SET", ""}};
        metadata.env = {{"SARUS_CONFIG_SET", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"SARUS_CONFIG_SET", ""}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));

        config->json["environment"].RemoveMember("set");
    }
    // cli re-sets unset variables
    {
        rj::Value unsetValue(rj::kArrayType);
        unsetValue.PushBack("SARUS_CONFIG_UNSET", allocator);
        config->json["environment"].AddMember("unset", unsetValue, allocator);

        config->commandRun.hostEnvironment = {};
        config->commandRun.userEnvironment = {{"SARUS_CONFIG_UNSET", "cli_value"}};
        metadata.env = {{"SARUS_CONFIG_UNSET", "IMAGE_VALUE"}};
        auto expectedEnvironment = std::unordered_map<std::string, std::string>{{"SARUS_CONFIG_UNSET", "cli_value"}};
        CHECK((ConfigsMerger{config, metadata}.getEnvironmentInContainer() == expectedEnvironment));

        config->json["environment"].RemoveMember("unset");
    }
}

void checkNvidiaEnvironmentVariables(const std::unordered_map<std::string, std::string>& resultEnvironment,
                                 const std::string expectedNvidiaVisibleDevices,
                                 const std::string expectedCudaVisibleDevices,
                                 const std::string expectedDriverCapabilities = "all") {
    if (expectedNvidiaVisibleDevices.empty()) {
        CHECK(resultEnvironment.find("CUDA_VISIBLE_DEVICES") == resultEnvironment.end());
        CHECK(resultEnvironment.find("NVIDIA_VISIBLE_DEVICES") == resultEnvironment.end());
        CHECK(resultEnvironment.find("NVIDIA_DRIVER_CAPABILITIES") == resultEnvironment.end());
    }
    else {
        CHECK(resultEnvironment.at("CUDA_VISIBLE_DEVICES") == expectedCudaVisibleDevices);
        CHECK(resultEnvironment.at("NVIDIA_VISIBLE_DEVICES") == expectedNvidiaVisibleDevices);
        CHECK(resultEnvironment.at("NVIDIA_DRIVER_CAPABILITIES") == expectedDriverCapabilities);
    }
}

TEST(ConfigsMergerTestGroup, nvidia_environment) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    auto metadata = common::ImageMetadata{};

    // Single device
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "0"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "0", "0");
    }
    // Single device, not 1st one, selected driver capabilities
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "1"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"NVIDIA_DRIVER_CAPABILITIES", "utility,compute"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "1", "0", "utility,compute");
    }
    // CUDA_VISIBLE_DEVICES in image
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "1"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"CUDA_VISIBLE_DEVICES", "0,1"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "1", "0");
    }
    // No host CUDA_VISIBLE_DEVICES
    {
        config->commandRun.hostEnvironment = {};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"NVIDIA_DRIVER_CAPABILITIES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "", "");
    }
    // Host CUDA_VISIBLE_DEVICES set to NoDevFiles
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "NoDevFiles"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"},{"NVIDIA_DRIVER_CAPABILITIES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "", "");
    }
    // Multiple devices in order
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "1,2"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "1,2", "0,1");
    }
    // Shuffled selection
    {
        config->commandRun.hostEnvironment = {{"CUDA_VISIBLE_DEVICES", "3,1,5"}};
        metadata.env = {{"NVIDIA_VISIBLE_DEVICES", "all"}};
        checkNvidiaEnvironmentVariables(ConfigsMerger{config, metadata}.getEnvironmentInContainer(),
                                    "3,1,5", "1,0,2");
    }
}

TEST(ConfigsMergerTestGroup, pmix_environment) {
    auto configRAII = test_utility::config::makeConfig();
    auto& config = configRAII.config;
    config->json.AddMember("enablePMIxv3Support", true, config->json.GetAllocator());
    auto metadata = common::ImageMetadata{};

    // Set variables
    {
        config->commandRun.hostEnvironment = {{"PMIX_PTL_MODULE", "pmix_ptl"}, {"PMIX_SECURITY_MODE", "pmix_security"},
                {"PMIX_GDS_MODULE", "pmix_gds"}};
        auto env = ConfigsMerger{config, metadata}.getEnvironmentInContainer();

        CHECK(env.at("PMIX_MCA_ptl") == std::string("pmix_ptl"));
        CHECK(env.at("PMIX_MCA_psec") == std::string("pmix_security"));
        CHECK(env.at("PMIX_MCA_gds") == std::string("pmix_gds"));
    }

    // Overwrite image variables
    {
        config->commandRun.hostEnvironment = {{"PMIX_SERVER_TMPDIR", "pmix_tmpdir"}, {"PMIX_PTL_MODULE", "pmix_ptl"},
                {"PMIX_SECURITY_MODE", "pmix_security"}, {"PMIX_GDS_MODULE", "pmix_gds"}};
        metadata.env =  {{"PMIX_SERVER_TMPDIR", "image_tmpdir"}, {"PMIX_PTL_MODULE", "image_ptl"},
                {"PMIX_SECURITY_MODE", "image_security"}, {"PMIX_GDS_MODULE", "image_gds"}, {"PMIX_iamge_only", "value"}};
        auto env = ConfigsMerger{config, metadata}.getEnvironmentInContainer();

        CHECK(env.at("PMIX_SERVER_TMPDIR") == std::string("pmix_tmpdir"));
        CHECK(env.at("PMIX_MCA_ptl") == std::string("pmix_ptl"));
        CHECK(env.at("PMIX_MCA_psec") == std::string("pmix_security"));
        CHECK(env.at("PMIX_MCA_gds") == std::string("pmix_gds"));
        CHECK(env.find("PMIX_image_only") == env.end());
    }

    // Skip unset or empty variables
    {
        config->commandRun.hostEnvironment = {{"PMIX_PTL_MODULE", "pmix_ptl"}, {"PMIX_GDS_MODULE", ""}};
        auto env = ConfigsMerger{config, metadata}.getEnvironmentInContainer();

        CHECK(env.at("PMIX_MCA_ptl") == std::string("pmix_ptl"));
        CHECK(env.find("PMIX_MCA_psec") == env.end());
        CHECK(env.find("PMIX_MCA_gds") == env.end());
    }

    // Do not modify PMIX_MCA variables if existing and non-empty
    {
        config->commandRun.hostEnvironment ={{"PMIX_PTL_MODULE", "pmix_ptl"}, {"PMIX_SECURITY_MODE", "pmix_security"},
                {"PMIX_GDS_MODULE", "pmix_gds"}, {"PMIX_MCA_ptl", "mca_ptl"}, {"PMIX_MCA_psec", ""}};
        auto env = ConfigsMerger{config, metadata}.getEnvironmentInContainer();

        CHECK(env.at("PMIX_MCA_ptl") == std::string("mca_ptl"));
        CHECK(env.at("PMIX_MCA_psec") == std::string("pmix_security"));
        CHECK(env.at("PMIX_MCA_gds") == std::string("pmix_gds"));
    }
}

TEST(ConfigsMergerTestGroup, bundle_annotations) {
    auto metadata = common::ImageMetadata{};

    // No hooks enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        auto expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "2"}
        };
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));
    }
    // glibc hook enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.enableGlibcReplacement = true;
        auto expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "2"},
            {"com.hooks.glibc.enabled", "true"}
        };
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));
    }
    // MPI hook enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.useMPI = true;
        auto expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "2"},
            {"com.hooks.glibc.enabled", "true"},
            {"com.hooks.mpi.enabled", "true"}
        };
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));

        // Explicit MPI type
        config->commandRun.useMPI = true;
        config->commandRun.mpiType = "mpi0";
        expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "2"},
            {"com.hooks.glibc.enabled", "true"},
            {"com.hooks.mpi.enabled", "true"},
            {"com.hooks.mpi.type", "mpi0"}
        };
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));
    }
    // SSH hook enabled
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.enableSSH = true;
        auto expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "2"},
            {"com.hooks.slurm-global-sync.enabled", "true"},
            {"com.hooks.ssh.enabled", "true"}
        };
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));
    }
    // Image annotations
    {
        metadata.labels["com.test.image.key"] = "image_value";
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        auto expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "2"},
            {"com.test.image.key", "image_value"}
        };
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));
        metadata.labels.erase("com.test.image.key");
    }
    // Custom annotations (from the CLI or from components like the FileDescriptoHandler)
    // override everything
    {
        metadata.labels["com.test.dummy_key"] = "image_value";
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.ociAnnotations["com.hooks.logging.level"] = "0";
        auto expectedAnnotations = std::unordered_map<std::string, std::string>{
            {"com.test.dummy_key", "dummy_value"},
            {"com.hooks.logging.level", "0"}
        };
        auto annot = ConfigsMerger{config, metadata}.getBundleAnnotations();
        CHECK((ConfigsMerger{config, metadata}.getBundleAnnotations() == expectedAnnotations));
    }
}

TEST(ConfigsMergerTestGroup, command_to_execute) {
    // init process
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.addInitProcess = true;
        config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
        auto metadata = common::ImageMetadata{};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"/dev/init", "--", "cmd-cli"}));
    }
    // only CLI cmd
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
        auto metadata = common::ImageMetadata{};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"cmd-cli"}));
    }
    // only metadata cmd
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.execArgs = common::CLIArguments{};
        auto metadata = common::ImageMetadata{};
        metadata.cmd = common::CLIArguments{"cmd-metadata"};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"cmd-metadata"}));
    }
    // CLI cmd overrides metadata cmd
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
        auto metadata = common::ImageMetadata{};
        metadata.cmd = common::CLIArguments{"cmd-metadata"};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"cmd-cli"}));
    }
    // only CLI entrypoint
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        config->commandRun.entrypoint = common::CLIArguments{"entry-cli"};
        auto metadata = common::ImageMetadata{};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-cli"}));
    }
    // only metadata entrypoint
    {
        auto configRAII = test_utility::config::makeConfig();
        auto& config = configRAII.config;
        auto metadata = common::ImageMetadata{};
        metadata.entry = common::CLIArguments{"entry-metadata"};
        CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-metadata"}));
    }
    // entrypoint + cmd
    {
        // metadata entrypoint + metadata cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            auto metadata = common::ImageMetadata{};
            metadata.cmd = common::CLIArguments{"cmd-metadata"};
            metadata.entry = common::CLIArguments{"entry-metadata"};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-metadata", "cmd-metadata"}));
        }
        // CLI entrypoint + CLI cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
            config->commandRun.entrypoint = common::CLIArguments{"entry-cli"};
            auto metadata = common::ImageMetadata{};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-cli", "cmd-cli"}));
        }
        // metadata entrypoint + CLI cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            config->commandRun.execArgs = common::CLIArguments{"cmd-cli"};
            auto metadata = common::ImageMetadata{};
            metadata.entry = common::CLIArguments{"entry-metadata"};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-metadata", "cmd-cli"}));
        }
        // CLI entrypoint overrides metadata entrypoint and metadata cmd
        {
            auto configRAII = test_utility::config::makeConfig();
            auto& config = configRAII.config;
            config->commandRun.entrypoint = common::CLIArguments{"entry-cli"};
            auto metadata = common::ImageMetadata{};
            metadata.cmd = common::CLIArguments{"cmd-metadata"};
            metadata.entry = common::CLIArguments{"entry-metadata"};
            CHECK((ConfigsMerger{config, metadata}.getCommandToExecuteInContainer() == common::CLIArguments{"entry-cli"}));
        }
    }
}

}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
