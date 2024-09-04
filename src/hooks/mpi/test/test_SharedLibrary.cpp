/*
 * Sarus
 *
 * Copyright (c) 2018-2023, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "libsarus/Logger.hpp"
#include "libsarus/Error.hpp"
#include "hooks/mpi/SharedLibrary.hpp"
#include "test_utility/unittest_main_function.hpp"
#include <vector>


namespace sarus {
namespace hooks {
namespace mpi {
namespace test {


TEST_GROUP(SharedLibraryTestGroup) {
};

TEST(SharedLibraryTestGroup, linkerName) {
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so"}.getLinkerName(), std::string{"libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so.1"}.getLinkerName(), std::string{"libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so.1.2"}.getLinkerName(), std::string{"libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so.1.2.3"}.getLinkerName(), std::string{"libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so.12.10.5"}.getLinkerName(), std::string{"libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so.12.5.5"}.getLinkerName(), std::string{"libfoo.so"});
}

TEST(SharedLibraryTestGroup, path) {
    CHECK_EQUAL(SharedLibrary{"/lib/foo/libfoo.so"}.getPath().string(), std::string{"/lib/foo/libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/usr/lib/libbar.so.1.2.3"}.getPath().string(), std::string{"/usr/lib/libbar.so.1.2.3"});
}

TEST(SharedLibraryTestGroup, realName) {
    CHECK_EQUAL(SharedLibrary{"/lib/foo/bar/libfoo.so.1.2.3"}.getRealName(), std::string{"libfoo.so.1.2.3"});
    CHECK_EQUAL(SharedLibrary{"/lib/foo/libfoo.so.1.2"}.getRealName(), std::string{"libfoo.so.1.2"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so.1"}.getRealName(), std::string{"libfoo.so.1"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo.so"}.getRealName(), std::string{"libfoo.so"});
    CHECK_EQUAL(SharedLibrary{"/lib/libfoo-2.0.so.0"}.getRealName(), std::string{"libfoo-2.0.so.0"});
}

TEST(SharedLibraryTestGroup, hasMajorVersion) {
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.1.2.3"}.hasMajorVersion());
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.1.2"}.hasMajorVersion());
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.1"}.hasMajorVersion());
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so"}.hasMajorVersion());
}

TEST(SharedLibraryTestGroup, fullAbiCompatibility) {
    // linkername diff
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so"}.isFullAbiCompatible(SharedLibrary{"/lib/libbar.so"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.1"}.isFullAbiCompatible(SharedLibrary{"/lib/libbar.so.1"}));

    // major diff
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.1"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.1"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.1"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.2"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.2"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.1"}));

    // minor bigger
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.4.4"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.3"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.4.5.1"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.4.4"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.12.10.5"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.12.5.5"}));

    // major equal, minor non bigger
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.1"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.1"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.1"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.1"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.2"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.1.7"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.2"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.2.7"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.2"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.2.8"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.3.1"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.12.200"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo.so.12.350.5"}));

    // patch number does not matter
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.2.8"}.isFullAbiCompatible(SharedLibrary{"/usr/lib/libfoo.so.4.3.1"}));

    // libs without minor are fullAbiCompatible
    CHECK_TRUE(SharedLibrary{"/lib/libfoo-2.0.so.0"}.isFullAbiCompatible(SharedLibrary{"/lib/libfoo-2.0.so.0"}));
}

TEST(SharedLibraryTestGroup, majorAbiCompatibility) {
    // linkername diff
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so"}.isMajorAbiCompatible(SharedLibrary{"/lib/libbar.so"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.2"}.isMajorAbiCompatible(SharedLibrary{"/lib/libbar.so.2"}));

    // major diff
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.4"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.3"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.4"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.5"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.4.4"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.3.3"}));
    CHECK_FALSE(SharedLibrary{"/lib/libfoo.so.4.4"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.5.5"}));

    // major equal
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.4"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.3"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.4"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.3"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.3"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.3"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.4.2"}));
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.3"}.isMajorAbiCompatible(SharedLibrary{"/lib/libfoo.so.4"}));

    // path does not matter
    CHECK_TRUE(SharedLibrary{"/lib/libfoo.so.4.3"}.isMajorAbiCompatible(SharedLibrary{"/usr/lib/libfoo.so.4"}));
}

TEST(SharedLibraryTestGroup, bestAbiMatch) {
    auto sl2 = SharedLibrary{"/lib/libfoo.so.2"};
    auto sl23 = SharedLibrary{"/lib/libfoo.so.2.3"};
    auto sl234 = SharedLibrary{"/lib/libfoo.so.2.3.4"};

    // exact matches
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so"}, SharedLibrary{"/lib/libfoo.so.2"}, SharedLibrary{"/lib/libfoo.so.3"}};
    auto best = sl2.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2"));
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2"}, SharedLibrary{"/lib/libfoo.so.2.4"}, SharedLibrary{"/lib/libfoo.so.2.3.4"}, SharedLibrary{"/lib/libfoo.so.2.3"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.3.4"));
    }

    // newest older
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2"}, SharedLibrary{"/lib/libfoo.so.2.3.3"}, SharedLibrary{"/lib/libfoo.so.2.3.2"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.3.3"));
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2.1"}, SharedLibrary{"/lib/libfoo.so.2.2"}, SharedLibrary{"/lib/libfoo.so.2.4"}};
    auto best = sl23.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.2"));
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2.1.7"}, SharedLibrary{"/lib/libfoo.so.2.2.3"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.2.3")); // minor is more important than patch
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2.3.3"}, SharedLibrary{"/lib/libfoo.so.2.3.6"}, SharedLibrary{"/lib/libfoo.so.2.3.5"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.3.6")); // don't downgrade patch
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2"}, SharedLibrary{"/lib/libfoo.so.2.3.7"}, SharedLibrary{"/lib/libfoo.so.3"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.3.7")); // newer patch is ok
    }

    // oldest newer
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.3"}, SharedLibrary{"/lib/libfoo.so.2.4"}, SharedLibrary{"/lib/libfoo.so.2.4.6"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.4.6"));
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.5"}, SharedLibrary{"/lib/libfoo.so.4"}, SharedLibrary{"/lib/libfoo.so.3.7"}};
    auto best = sl234.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.3.7"));
    }

    // corner cases
    auto candidates = std::vector<SharedLibrary>{};
    CHECK_THROWS(libsarus::Error, sl2.pickNewestAbiCompatibleLibrary(candidates));
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2"}};
    auto best = sl2.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2"));  // exact match with just major
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.4"}};
    auto best = sl2.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.4"));  // only candidate non compatible still "best"
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.1"}, SharedLibrary{"/lib/libfoo.so.3"}};
    auto best = sl2.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.1"));  // though incompatible, still newest older
    }
    {
    auto candidates = std::vector<SharedLibrary>{SharedLibrary{"/lib/libfoo.so.2.10"}, SharedLibrary{"/lib/libfoo.so.2.2"}};
    auto best = SharedLibrary{"/lib/libfoo.so.2.20"}.pickNewestAbiCompatibleLibrary(candidates);
    CHECK_EQUAL(best.getRealName(), std::string("libfoo.so.2.10")); // check for typical string-comparison pitfalls
    }
}

TEST(SharedLibraryTestGroup, areMajorCompatible) {
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so"},SharedLibrary{"libfoo.so"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2"},SharedLibrary{"libfoo.so.2"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2.10"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.10.5"}));

    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.11"},SharedLibrary{"libfoo.so.2.10"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.11.5"},SharedLibrary{"libfoo.so.2.10.5"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10.6"},SharedLibrary{"libfoo.so.2.10.5"}));

    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2.11"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.11.5"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.10.6"}));

    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2"},SharedLibrary{"libfoo.so.2.10.5"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2.10.5"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2"},SharedLibrary{"libfoo.so.2.10"}));
    
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.10"}));
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2"}));
}

TEST(SharedLibraryTestGroup, areFullCompatible) {
    CHECK(areMajorAbiCompatible(SharedLibrary{"libfoo.so"},SharedLibrary{"libfoo.so"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2"},SharedLibrary{"libfoo.so.2"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2.10"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.10.5"}));

    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.11"},SharedLibrary{"libfoo.so.2.10"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.11.5"},SharedLibrary{"libfoo.so.2.10.5"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10.6"},SharedLibrary{"libfoo.so.2.10.5"}));

    CHECK_FALSE(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2.11"}));
    CHECK_FALSE(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.11.5"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.10.6"}));

    CHECK_FALSE(areFullAbiCompatible(SharedLibrary{"libfoo.so.2"},SharedLibrary{"libfoo.so.2.10.5"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2.10.5"}));
    CHECK_FALSE(areFullAbiCompatible(SharedLibrary{"libfoo.so.2"},SharedLibrary{"libfoo.so.2.10"}));
    
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10.5"},SharedLibrary{"libfoo.so.2.10"}));
    CHECK(areFullAbiCompatible(SharedLibrary{"libfoo.so.2.10"},SharedLibrary{"libfoo.so.2"}));
}

}}}} // namespace

SARUS_UNITTEST_MAIN_FUNCTION();
