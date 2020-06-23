/*
 * Sarus
 *
 * Copyright (c) 2018-2020, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "ConfigsMerger.hpp"

#include <boost/algorithm/string.hpp>

#include "runtime/Utility.hpp"

namespace rj = rapidjson;

namespace sarus {
namespace runtime {

ConfigsMerger::ConfigsMerger(std::shared_ptr<const common::Config> config, const common::ImageMetadata& metadata)
    : config{std::move(config)}
    , metadata{metadata}
{}

boost::filesystem::path ConfigsMerger::getWorkdirInContainer() const {
    if(config->commandRun.workdir) {
        return *config->commandRun.workdir;
    }
    else if(metadata.workdir) {
        return *metadata.workdir;
    }
    else {
        return "/";
    }
}

std::unordered_map<std::string, std::string> ConfigsMerger::getEnvironmentInContainer() const {
    auto env = config->commandRun.hostEnvironment;
    for(const auto& kv : metadata.env) {
        env[kv.first] = kv.second;
    }
    setNvidiaEnvironmentVariables(config->commandRun.hostEnvironment, env);
    return env;
}

std::unordered_map<std::string, std::string> ConfigsMerger::getBundleAnnotations() const {
    auto annotations = config->commandRun.bundleAnnotations;

    if(config->commandRun.enableGlibcReplacement) {
        annotations["com.hooks.glibc.enabled"] = "true";
    }

    if(config->commandRun.useMPI) {
        annotations["com.hooks.glibc.enabled"] = "true";
        annotations["com.hooks.mpi.enabled"] = "true";
    }

    if(config->commandRun.enableSSH) {
        annotations["com.hooks.slurm-global-sync.enabled"] = "true";
        annotations["com.hooks.ssh.enabled"] = "true";
    }

    using IntType = typename std::underlying_type<common::LogLevel>::type;
    auto level = static_cast<IntType>(common::Logger::getInstance().getLevel());
    annotations["com.hooks.logging.level"] = std::to_string(level);

    return annotations;
}

/**
 * Sets the container environment variables read by the nvidia-container-runtime hook
 * (i.e. NVIDIA_VISIBLE_DEVICES and NVIDIA_DRIVER_CAPABILITIES) to values compatible with
 * the devices assigned by the host through CUDA_VISIBLE_DEVICES (which can be set by the
 * workload manager, e.g. SLURM's GRES plugin).
 * This prevents images that set NVIDIA_VISIBLE_DEVICES (e.g. NVIDIA official DockerHub images)
 * to override the allocation of the workload manager.
 * It is also important to avoid overrides from CUDA_VISIBLE_DEVICES set in the container image,
 * or from CUDA_VERSION (which, in images pre-nvidia-dockerv2 will default the hook to load all
 * devices and driver capabilities).
 *
 * If a container is assigned GPUs but its image has not set NVIDIA_DRIVER_CAPABILITIES
 * (e.g. legacy nvidia-docker version 1 images), all driver capabilities will be enabled by
 * this function.
 *
 * In a multi-GPU system, a partial or shuffled selection of GPUs through CUDA_VISIBLE_DEVICES
 * will most likely result in a CUDA_VISIBLE_DEVICES that is no longer valid inside the container,
 * since the variable has been set in the context of the host.
 * For example, a CUDA_VISIBLE_DEVICES=1 on the host will be the only device detectable by the driver
 * inside the container, and thus will have to be referenced by CUDA_VISIBLE_DEVICES=0 in order
 * for the CUDA runtime to see it.
 * In the same way, a host CUDA_VISIBLE_DEVICES=3,1,5 will have to be converted to a value of 1,0,2 inside
 * the container. *
 */
void ConfigsMerger::setNvidiaEnvironmentVariables(const std::unordered_map<std::string, std::string>& hostEnvironment,
                                                    std::unordered_map<std::string, std::string>& containerEnvironment
                                                    ) const {
    const auto gpuDevicesAvailable = hostEnvironment.find("CUDA_VISIBLE_DEVICES");
    if (gpuDevicesAvailable != hostEnvironment.end() && gpuDevicesAvailable->second != "NoDevFiles") {
        containerEnvironment["NVIDIA_VISIBLE_DEVICES"] = gpuDevicesAvailable->second;
        if (containerEnvironment.find("NVIDIA_DRIVER_CAPABILITIES") == containerEnvironment.end()) {
            containerEnvironment["NVIDIA_DRIVER_CAPABILITIES"] = "all";
        }
        // Adapt CUDA_VISIBLE_DEVICES (here CVD, for short) taking into account possible shuffles
        // Given a device index on the host, the correct index inside the container can be obtained
        // using a sorted copy of the host CVD: the container index will be the position of the
        // corresponding host index in the sorted list.
        auto hostCVD = std::vector<std::string>{};
        boost::split(hostCVD, gpuDevicesAvailable->second, boost::is_any_of(","));
        auto hostCVDSorted = hostCVD;
        std::sort(hostCVDSorted.begin(), hostCVDSorted.end());
        std::string containerCVD;
        for (const auto& hostDeviceID: hostCVD) {
            auto containerDeviceItr = std::find(hostCVDSorted.begin(), hostCVDSorted.end(), hostDeviceID);
            size_t containerDeviceIdx = std::distance(hostCVDSorted.begin(), containerDeviceItr);
            containerCVD += std::to_string(containerDeviceIdx) + ',';
        }
        containerCVD.pop_back();
        containerEnvironment["CUDA_VISIBLE_DEVICES"] = containerCVD;
    }
    else {
        containerEnvironment.erase("CUDA_VERSION");
        containerEnvironment.erase("CUDA_VISIBLE_DEVICES");
        containerEnvironment.erase("NVIDIA_VISIBLE_DEVICES");
        containerEnvironment.erase("NVIDIA_DRIVER_CAPABILITIES");
    }
}

common::CLIArguments ConfigsMerger::getCommandToExecuteInContainer() const {
    utility::logMessage("Building command to execute in container", common::LogLevel::INFO);

    auto result = common::CLIArguments{};

    // first of all init program (if requested)
    if(config->commandRun.addInitProcess) {
        result.push_back("/dev/init");
        result.push_back("--");
    }
    // then entrypoint (if any) (CLI entrypoint has priority over metadata/image entrypoint)
    if(config->commandRun.entrypoint) {
        result += *config->commandRun.entrypoint;
    }
    else if(metadata.entry) {
        result += *metadata.entry;
    }
    // then cmd (CLI command has priority over metadata/image command)
    if(!config->commandRun.execArgs.empty()) {
        result += config->commandRun.execArgs;
    }
    else if(metadata.cmd && !config->commandRun.entrypoint) { // CLI entrypoint overrides metadata/image command
        result += *metadata.cmd;
    }
    else if(result.empty()) {
        SARUS_THROW_ERROR("Failed to determine the command to execute in the container."
                            " At least one command or an entry point should be specified"
                            " through the CLI arguments or the image metadata.");
    }

    utility::logMessage(
        boost::format("Successfully built command to execute in container: %s") % result,
        common::LogLevel::INFO);

    return result;
}

}
}
