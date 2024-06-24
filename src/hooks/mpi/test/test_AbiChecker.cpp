/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Error.hpp"
#include "libsarus/Logger.hpp"
#include "hooks/mpi/AbiChecker.hpp"

#include "test_utility/unittest_main_function.hpp"

#include <boost/type_index.hpp>

namespace sarus {
namespace hooks {
namespace mpi {
namespace test {

TEST_GROUP(AbiCheckerTestGroup){
  std::pair<bool, boost::optional<boost::format>> areCompatible;
};

TEST_GROUP(AbiCheckerFactoryTestGroup){};

TEST(AbiCheckerTestGroup, all_mpi_checkers_throw_if_no_major_compatible) {
  {
    MajorAbiCompatibilityChecker checker;
    CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.2"}));
  }
  {
    FullAbiCompatibilityChecker checker;
    CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.2"}));
  }
  {
    StrictAbiCompatibilityChecker checker;
    CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.2"}));
  }
}

TEST(AbiCheckerTestGroup, majorAbiCompatibility_succeed_if_are_equal) {
  MajorAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, majorAbiCompatibility_succeed_if_minor_or_release_number_is_higher_than_container) {
  MajorAbiCompatibilityChecker checker;
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.3"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.4"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, majorAbiCompatibility_succeed_if_minor_or_release_number_is_lower_than_container) {
  MajorAbiCompatibilityChecker checker;
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
  
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1.2.4"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, majorAbiCompatibility_with_no_minor_and_release_number) {
  MajorAbiCompatibilityChecker checker;
  
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
  
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, fullAbiCompatibility_succeed_if_are_equal) {
  FullAbiCompatibilityChecker checker;
  
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
  
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, fullAbiCompatibility_succeed_if_minor_or_release_number_is_higher_than_container) {
  FullAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.3"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
  
  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.4"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, fullAbiCompatibility_complains_if_minor_number_is_lower_than_container) {
  FullAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second != boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1.2.4"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, fullAbiCompatibility_complains_with_no_minor_number_in_host) {
  FullAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second != boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second != boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, fullAbiCompatibility_with_no_minor_number_in_container_succeed) {
  FullAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, strictAbiCompatibility_succeed_if_are_equal) {
  StrictAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, strictAbiCompatibility_ignores_patch_number) {
  StrictAbiCompatibilityChecker checker;

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.0"}, SharedLibrary{"/lib/libfoo.so.1.2.1"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.4"}, SharedLibrary{"/lib/libfoo.so.1.2.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}


TEST(AbiCheckerTestGroup, strictAbiCompatibility_throws_if_mojor_or_minor_are_different) {
  StrictAbiCompatibilityChecker checker;

  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.2"}));
  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.3"}, SharedLibrary{"/lib/libfoo.so.2"}));

  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.3"}));
  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1.3"}, SharedLibrary{"/lib/libfoo.so.1.2"}));
}

TEST(AbiCheckerTestGroup, strictAbiCompatibility_throws_with_no_minor_number) {
  StrictAbiCompatibilityChecker checker;
  
  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.1.2"}));
  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.1.2.3"}));

  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1"}));
  CHECK_THROWS(libsarus::Error, checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1"}));

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2"}, SharedLibrary{"/lib/libfoo.so.1.2.3"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);

  areCompatible = checker.check(SharedLibrary{"/lib/libfoo.so.1.2.3"}, SharedLibrary{"/lib/libfoo.so.1.2"});
  CHECK_TRUE(areCompatible.first);
  CHECK_TRUE(areCompatible.second == boost::none);
}

TEST(AbiCheckerTestGroup, dependenciesAbiCompatibility_true_if_major_compatible_and_silent_if_fully) {
  DependenciesAbiCompatibilityChecker checker;

  for (std::pair<std::string, std::string> libs : std::vector<std::pair<std::string, std::string>>{
           {"/lib/libfoo.so.1.2", "/lib/libfoo.so.1.2"},
           {"/lib/libfoo.so.1.2.3", "/lib/libfoo.so.1.2.3"},
           {"/lib/libfoo.so.1.3", "/lib/libfoo.so.1.2"},
           {"/lib/libfoo.so.1.2.4", "/lib/libfoo.so.1.2.3"},
           {"/lib/libfoo.so.1.2.3", "/lib/libfoo.so.1.2.4"},
          }) {
    CHECK_TRUE(checker.check(SharedLibrary{libs.first}, SharedLibrary{libs.second}).first);
    CHECK_TRUE(checker.check(SharedLibrary{libs.first}, SharedLibrary{libs.second}).second == boost::none);
  }

  for (std::pair<std::string, std::string> libs : std::vector<std::pair<std::string, std::string>>{
           {"/lib/libfoo.so.1.2", "/lib/libfoo.so.1.3"},
           {"/lib/libfoo.so.1", "/lib/libfoo.so.1.2"},
           {"/lib/libfoo.so.1", "/lib/libfoo.so.1.2.3"}
          }) {
    CHECK_TRUE(checker.check(SharedLibrary{libs.first}, SharedLibrary{libs.second}).first);
    CHECK_FALSE(checker.check(SharedLibrary{libs.first}, SharedLibrary{libs.second}).second == boost::none);
  }

  for (std::pair<std::string, std::string> libs : std::vector<std::pair<std::string, std::string>>{
           {"/lib/libfoo.so", "/lib/libfoo.so.1"},
           {"/lib/libfoo.so.1", "/lib/libfoo.so.2"},
           {"/lib/libfoo.so.2", "/lib/libfoo.so.1"}
           }) {
    CHECK_FALSE(checker.check(SharedLibrary{libs.first}, SharedLibrary{libs.second}).first);            
    CHECK_FALSE(checker.check(SharedLibrary{libs.first}, SharedLibrary{libs.second}).second == boost::none);
  }
}

TEST(AbiCheckerFactoryTestGroup, all_names_are_in_map) {
  AbiCheckerFactory factory;
  bool compareTypes{factory.getCheckerTypes() == std::set<std::string>{"major", "full", "strict", "dependencies"}};
  CHECK_TRUE(compareTypes);
}

TEST(AbiCheckerFactoryTestGroup, return_types_are_correct) {
  AbiCheckerFactory factory;
  CHECK_EQUAL(typeid(*factory.create("major")).name(), typeid(MajorAbiCompatibilityChecker).name());
  CHECK_EQUAL(typeid(*factory.create("full")).name(), typeid(FullAbiCompatibilityChecker).name());
  CHECK_EQUAL(typeid(*factory.create("strict")).name(), typeid(StrictAbiCompatibilityChecker).name());
  CHECK_EQUAL(typeid(*factory.create("dependencies")).name(), typeid(DependenciesAbiCompatibilityChecker).name());
}

} // namespace test
} // namespace mpi
} // namespace hooks
} // namespace sarus

SARUS_UNITTEST_MAIN_FUNCTION();
