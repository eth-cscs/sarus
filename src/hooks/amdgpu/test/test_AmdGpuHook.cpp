/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/PathRAII.hpp"
#include "libsarus/Utility.hpp"
#include "test_utility/Misc.hpp"
#include "test_utility/OCIHooks.hpp"
#include "test_utility/config.hpp"
#include "test_utility/unittest_main_function.hpp"

namespace sarus {
namespace hooks {
namespace amdgpu {

namespace fs = boost::filesystem;

extern std::vector<std::string> getRocrVisibleDevicesId(
    const fs::path& bundleDir);
extern std::unordered_map<std::string, std::string> mapDevicesIdToRenderD(
    const std::string&);
extern std::vector<std::string> getRenderDDevices(const std::string&,
                                                  const fs::path& bundleDir);

namespace test {

namespace fs = boost::filesystem;

void createDriSubdir(const fs::path& path, std::vector<int> ids) {
  if (fs::exists(path)) {
    fs::remove_all(path);
  }
  libsarus::createFoldersIfNecessary(path / "by-path");

  int busId = 193;
  for (auto id : ids) {
    libsarus::createFileIfNecessary(path /
                                         (boost::format("card%1%") % id).str());
    libsarus::createFileIfNecessary(
        path / (boost::format("renderD%1%") % (128 + id)).str());

    fs::create_symlink(
        fs::path((boost::format("../card%1%") % id).str()),
        path /
            (boost::format("by-path/pci-0000:%1$x:00.0-card%2%") % busId % id)
                .str());
    fs::create_symlink(
        fs::path((boost::format("../renderD%1%") % (128 + id)).str()),
        path / (boost::format("by-path/pci-0000:%1$x:00.0-render%2%") %
                (busId) % id)
                   .str());
    busId += 2;
  }
}

void createOCIBundleConfigJSON(const boost::filesystem::path& bundleDir,
                               const std::string& rocrVisibleDevices,
                               const std::tuple<uid_t, gid_t>& idsOfUser) {
  namespace rj = rapidjson;
  auto doc = test_utility::ocihooks::createBaseConfigJSON(bundleDir / "rootfs",
                                                          idsOfUser);
  auto& allocator = doc.GetAllocator();
  if (!rocrVisibleDevices.empty()) {
    doc["process"]["env"].PushBack(
        rj::Value{rocrVisibleDevices.c_str(), allocator}, allocator);
  }
  libsarus::writeJSON(doc, bundleDir / "config.json");
}

TEST_GROUP(AMDGPUHookTestGroup) {
  std::tuple<uid_t, gid_t> idsOfUser = test_utility::misc::getNonRootUserIds();
  test_utility::config::ConfigRAII configRAII =
      test_utility::config::makeConfig();
  boost::filesystem::path bundleDir =
      configRAII.config->json["OCIBundleDir"].GetString();
};

TEST(AMDGPUHookTestGroup,
     getRocrVisibleDevicesId_matches_ROCR_VISIBLE_DEVICES) {
  createOCIBundleConfigJSON(bundleDir, "ROCR_VISIBLE_DEVICES=0,1,2", idsOfUser);
  CHECK(getRocrVisibleDevicesId(bundleDir) ==
        (std::vector<std::string>{"0", "1", "2"}));
  createOCIBundleConfigJSON(bundleDir, "ROCR_VISIBLE_DEVICES=0,1,3", idsOfUser);
  CHECK(getRocrVisibleDevicesId(bundleDir) ==
        (std::vector<std::string>{"0", "1", "3"}));
}

TEST(AMDGPUHookTestGroup,
     getRocrVisibleDevicesId_is_empty_if_ROCR_VISIBLE_DEVICES_is_not_present) {
  CHECK_EQUAL(getRocrVisibleDevicesId(bundleDir).size(), 0);
}

void checkContainsAllAndOnly(std::vector<std::string> expected,
                             std::vector<std::string> found) {
  for (auto v : found) {
    auto element = std::find(expected.begin(), expected.end(), v);
    CHECK(element != expected.end());
    expected.erase(element);
  }
  CHECK(expected.empty());
}

std::vector<std::string> getExpectedDeviceFiles(
    const std::vector<int>& cardNumbers,
    const fs::path& prefix) {
  std::vector<std::string> expected;
  for (auto cardNumber : cardNumbers) {
    expected.emplace_back(
        (prefix / (boost::format("card%1%") % cardNumber).str()).string());
    expected.emplace_back(
        (prefix / (boost::format("renderD%1%") % (128 + cardNumber)).str())
            .string());
  }
  return expected;
}

TEST(AMDGPUHookTestGroup,
     find_all_render_devices_if_ROCR_VISIBLE_DEVICES_is_not_defined) {
  auto mockDriPath = libsarus::PathRAII(
                         libsarus::makeUniquePathWithRandomSuffix(
                             boost::filesystem::current_path() / "mockDri"))
                         .getPath();
  createDriSubdir(mockDriPath, {0, 1, 2, 3});

  {
    auto mountPoints{getRenderDDevices(mockDriPath.string(), bundleDir)};
    checkContainsAllAndOnly(getExpectedDeviceFiles({0, 1, 2, 3}, mockDriPath),
                            mountPoints);
  }
}

TEST(AMDGPUHookTestGroup, find_all_render_devices_in_ROCR_VISIBLE_DEVICES) {
  auto mockDriPath = libsarus::PathRAII(
                         libsarus::makeUniquePathWithRandomSuffix(
                             boost::filesystem::current_path() / "mockDri"))
                         .getPath();
  createDriSubdir(mockDriPath, {0, 1, 2, 3});

  {
    createOCIBundleConfigJSON(bundleDir, "ROCR_VISIBLE_DEVICES=0,1,2",
                              idsOfUser);
    auto mountPoints{getRenderDDevices(mockDriPath.string(), bundleDir)};
    checkContainsAllAndOnly(getExpectedDeviceFiles({0, 1, 2}, mockDriPath),
                            mountPoints);
  }

  {
    createOCIBundleConfigJSON(bundleDir, "ROCR_VISIBLE_DEVICES=0,1,3",
                              idsOfUser);
    auto mountPoints{getRenderDDevices(mockDriPath.string(), bundleDir)};
    checkContainsAllAndOnly(getExpectedDeviceFiles({0, 1, 3}, mockDriPath),
                            mountPoints);
  }
}

}  // namespace test
}  // namespace amdgpu
}  // namespace hooks
}  // namespace sarus

SARUS_UNITTEST_MAIN_FUNCTION();
