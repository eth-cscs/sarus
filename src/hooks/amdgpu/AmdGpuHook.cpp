/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "hooks/amdgpu/AmdGpuHook.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "libsarus/Utility.hpp"

namespace sarus {
namespace hooks {
namespace amdgpu {

namespace fs = boost::filesystem;

std::unordered_map<std::string, std::string> mapDevicesIdToRenderD(
    const std::string& path) {
  std::unordered_map<std::string, std::string> devicesIdToRenderD;
  const fs::directory_iterator end_itr;
  std::string cardId;

  fs::path byPath(path + "/by-path");
  for (fs::directory_iterator itr(byPath); itr != end_itr; ++itr) {
    if (!libsarus::filesystem::isSymlink(itr->path()) ||
        itr->path().string().find("card") == std::string::npos)
      continue;

    cardId = fs::path(fs::read_symlink(itr->path())).filename().string();

    libsarus::string::replace(cardId, "card", "");

    auto renderNName{itr->path().string()};
    libsarus::string::replace(renderNName, "card", "render");
    auto renderNPath{fs::path(renderNName)};

    try {
      devicesIdToRenderD.emplace(
          cardId,
          fs::canonical(fs::absolute(fs::read_symlink(renderNPath), byPath))
              .string());
    } catch (std::exception& e) {
      libsarus::Logger::getInstance().log(e.what(), "AMD GPU hook",
                                               libsarus::LogLevel::WARN);
    }
  }
  return devicesIdToRenderD;
}

std::vector<std::string> getRocrVisibleDevicesId(const fs::path& bundleDir) {
  std::string rocrVisibleDevices;
  try {
    auto containerEnvironment =
        libsarus::hook::parseEnvironmentVariablesFromOCIBundle(
            bundleDir);
    rocrVisibleDevices = containerEnvironment["ROCR_VISIBLE_DEVICES"];
  } catch (libsarus::Error& e) {
    libsarus::Logger::getInstance().log(e.what(), "AMD GPU hook",
                                             libsarus::LogLevel::INFO);
  }

  std::vector<std::string> rocrVisibleDevicesId;
  if (rocrVisibleDevices.empty()) {
    return rocrVisibleDevicesId;
  }
  boost::split(rocrVisibleDevicesId, rocrVisibleDevices, boost::is_any_of(","));

  return rocrVisibleDevicesId;
}

std::vector<std::string> getRenderDDevices(const std::string& path,
                                           const fs::path& bundleDir) {
  auto renderDMapping{mapDevicesIdToRenderD(path)};
  auto visibleDevicesIds{getRocrVisibleDevicesId(bundleDir)};
  std::vector<std::string> devices;

  if (visibleDevicesIds.empty()) {
    for (auto device : renderDMapping) {
      devices.emplace_back(path + "/card" + device.first);
      devices.push_back(device.second);
    }
    return devices;
  }

  for (auto id : visibleDevicesIds) {
    devices.emplace_back(path + "/card" + id);
    devices.push_back(renderDMapping.at(id));
  }
  return devices;
}

AmdGpuHook::AmdGpuHook() {
  log("Initializing hook", libsarus::LogLevel::INFO);

  containerState = libsarus::hook::parseStateOfContainerFromStdin();
  parseConfigJSONOfBundle();

  log("Successfully initialized hook", libsarus::LogLevel::INFO);
}

void AmdGpuHook::activate() {
  log("Activating AMD GPU support", libsarus::LogLevel::INFO);

  try {
    if (!libsarus::filesystem::isDeviceFile(fs::path{"/dev/kfd"})) {
      return;
    }
  } catch (const libsarus::Error& e) {
    log(e.what(), libsarus::LogLevel::INFO);
    return;
  }
  performBindMounts();

  log("Successfully activated AMD GPU support", libsarus::LogLevel::INFO);
}

void AmdGpuHook::parseConfigJSONOfBundle() {
  log("Parsing bundle's config.json", libsarus::LogLevel::INFO);

  auto json = libsarus::json::read(containerState.bundle() / "config.json");

  libsarus::hook::applyLoggingConfigIfAvailable(json);

  auto root = fs::path{json["root"]["path"].GetString()};
  if (root.is_absolute()) {
    rootfsDir = root;
  } else {
    rootfsDir = containerState.bundle() / root;
  }

  auto uidOfUser{json["process"]["user"]["uid"].GetInt()};
  auto gidOfUser{json["process"]["user"]["gid"].GetInt()};
  userIdentity = libsarus::UserIdentity(uidOfUser, gidOfUser, {});

  log("Successfully parsed bundle's config.json",
      libsarus::LogLevel::INFO);
}

void AmdGpuHook::performBindMounts() const {
  log("Performing bind mounts", libsarus::LogLevel::INFO);
  auto devicesCgroupPath = fs::path{};

  std::vector<std::string> mountPoints{
      getRenderDDevices("/dev/dri", containerState.bundle())};
  mountPoints.emplace_back("/dev/kfd");

  for (const auto& mountPoint : mountPoints) {
    if (mountPoint.empty())
      continue;

    libsarus::mount::validatedBindMount(mountPoint, mountPoint, userIdentity,
                                       rootfsDir);

    if (libsarus::filesystem::isDeviceFile(mountPoint)) {
      if (devicesCgroupPath.empty()) {
        devicesCgroupPath =
            libsarus::hook::findCgroupPath("devices", "/", containerState.pid());
      }

      libsarus::hook::whitelistDeviceInCgroup(devicesCgroupPath, mountPoint);
    }
  }

  log("Successfully performed bind mounts", libsarus::LogLevel::INFO);
}

void AmdGpuHook::log(const std::string& message,
                     libsarus::LogLevel level) const {
  libsarus::Logger::getInstance().log(message, "AMD GPU hook", level);
}

void AmdGpuHook::log(const boost::format& message,
                     libsarus::LogLevel level) const {
  libsarus::Logger::getInstance().log(message.str(), "AMD GPU hook",
                                           level);
}

}  // namespace amdgpu
}  // namespace hooks
}  // namespace sarus
