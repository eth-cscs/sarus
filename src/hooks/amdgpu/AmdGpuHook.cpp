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

#include "common/Utility.hpp"
#include "hooks/common/Utility.hpp"
#include "runtime/mount_utilities.hpp"

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
    if (!sarus::common::isSymlink(itr->path()) ||
        itr->path().string().find("card") == std::string::npos)
      continue;

    cardId = fs::path(fs::read_symlink(itr->path())).filename().string();

    sarus::common::replaceString(cardId, "card", "");

    auto renderNName{itr->path().string()};
    sarus::common::replaceString(renderNName, "card", "render");
    auto renderNPath{fs::path(renderNName)};

    try {
      devicesIdToRenderD.emplace(
          cardId,
          fs::canonical(fs::absolute(fs::read_symlink(renderNPath), byPath))
              .string());
    } catch (std::exception& e) {
      sarus::common::Logger::getInstance().log(e.what(), "AMD GPU hook",
                                               sarus::common::LogLevel::WARN);
    }
  }
  return devicesIdToRenderD;
}

std::vector<std::string> getRocrVisibleDevicesId(const fs::path& bundleDir) {
  std::string rocrVisibleDevices;
  try {
    auto containerEnvironment =
        hooks::common::utility::parseEnvironmentVariablesFromOCIBundle(
            bundleDir);
    rocrVisibleDevices = containerEnvironment["ROCR_VISIBLE_DEVICES"];
  } catch (sarus::common::Error& e) {
    sarus::common::Logger::getInstance().log(e.what(), "AMD GPU hook",
                                             sarus::common::LogLevel::INFO);
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
  log("Initializing hook", sarus::common::LogLevel::INFO);

  std::tie(bundleDir, pidOfContainer) =
      hooks::common::utility::parseStateOfContainerFromStdin();
  sarus::hooks::common::utility::enterMountNamespaceOfProcess(pidOfContainer);
  parseConfigJSONOfBundle();

  log("Successfully initialized hook", sarus::common::LogLevel::INFO);
}

void AmdGpuHook::activate() {
  log("Activating AMD GPU support", sarus::common::LogLevel::INFO);

  try {
    if (!sarus::common::isDeviceFile(fs::path{"/dev/kfd"})) {
      return;
    }
  } catch (const sarus::common::Error& e) {
    log(e.what(), sarus::common::LogLevel::INFO);
    return;
  }
  performBindMounts();

  log("Successfully activated AMD GPU support", sarus::common::LogLevel::INFO);
}

void AmdGpuHook::parseConfigJSONOfBundle() {
  log("Parsing bundle's config.json", sarus::common::LogLevel::INFO);

  auto json = sarus::common::readJSON(bundleDir / "config.json");

  hooks::common::utility::applyLoggingConfigIfAvailable(json);

  auto root = fs::path{json["root"]["path"].GetString()};
  if (root.is_absolute()) {
    rootfsDir = root;
  } else {
    rootfsDir = bundleDir / root;
  }

  auto uidOfUser{json["process"]["user"]["uid"].GetInt()};
  auto gidOfUser{json["process"]["user"]["gid"].GetInt()};
  userIdentity = sarus::common::UserIdentity(uidOfUser, gidOfUser, {});

  log("Successfully parsed bundle's config.json",
      sarus::common::LogLevel::INFO);
}

void AmdGpuHook::performBindMounts() const {
  log("Performing bind mounts", sarus::common::LogLevel::INFO);
  auto devicesCgroupPath = fs::path{};

  std::vector<std::string> mountPoints{
      getRenderDDevices("/dev/dri", bundleDir)};
  mountPoints.emplace_back("/dev/kfd");

  for (const auto& mountPoint : mountPoints) {
    if (mountPoint.empty())
      continue;

    sarus::runtime::validatedBindMount(mountPoint, mountPoint, userIdentity,
                                       rootfsDir);

    if (sarus::common::isDeviceFile(mountPoint)) {
      if (devicesCgroupPath.empty()) {
        devicesCgroupPath =
            common::utility::findCgroupPath("devices", "/", pidOfContainer);
      }

      common::utility::whitelistDeviceInCgroup(devicesCgroupPath, mountPoint);
    }
  }

  log("Successfully performed bind mounts", sarus::common::LogLevel::INFO);
}

void AmdGpuHook::log(const std::string& message,
                     sarus::common::LogLevel level) const {
  sarus::common::Logger::getInstance().log(message, "AMD GPU hook", level);
}

void AmdGpuHook::log(const boost::format& message,
                     sarus::common::LogLevel level) const {
  sarus::common::Logger::getInstance().log(message.str(), "AMD GPU hook",
                                           level);
}

}  // namespace amdgpu
}  // namespace hooks
}  // namespace sarus
