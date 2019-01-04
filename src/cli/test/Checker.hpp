#ifndef sarus_cli_test_Checker_hpp
#define sarus_cli_test_Checker_hpp

#include <memory>
#include <algorithm>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "test_utility/config.hpp"
#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "common/Error.hpp"
#include "cli/MountParser.hpp"
#include "runtime/UserMount.hpp"
#include "runtime/SiteMount.hpp"

#include <CppUTest/CommandLineTestRunner.h> // boost library must be included before CppUTest


namespace sarus {
namespace cli {
namespace custom_mounts {
namespace test {

class Checker {
public:
    Checker(const std::string& mountRequest)
        : mountRequest(mountRequest)
    {}

    Checker& parseAsSiteMount() {
        this->isSiteMount = true;
        return *this;
    }

    Checker& expectSource(const std::string& expectedSource) {
        this->expectedSource = expectedSource;
        return *this;
    }

    Checker& expectDestination(const std::string& expectedDestination) {
        this->expectedDestination = expectedDestination;
        return *this;
    }

    Checker& expectFlags(const size_t flags) {
        this->expectedFlags = flags;
        return *this;
    }

    Checker& expectParseError() {
        isParseErrorExpected = true;
        return *this;
    }

    ~Checker() {
        auto config = std::make_shared<common::Config>(test_utility::config::makeConfig());
        auto mountObject = std::unique_ptr<runtime::Mount>{};

        auto parser = cli::MountParser{!isSiteMount, config};

        auto map = common::convertListOfKeyValuePairsToMap(mountRequest);

        if(isParseErrorExpected) {
            CHECK_THROWS(sarus::common::Error, parser.parseMountRequest(map));
            return;
        }

        mountObject = parser.parseMountRequest(map);

        boost::optional<boost::filesystem::path> actualSource;
        boost::optional<boost::filesystem::path> actualDestination;
        boost::optional<size_t> actualFlags = 0;
        boost::optional<std::vector<int>> actualSiteFlagsSet = {};

        if(dynamic_cast<runtime::UserMount*>(mountObject.get())) {
            const auto* p = dynamic_cast<runtime::UserMount*>(mountObject.get());
            actualSource = p->source;
            actualDestination = p->destination;
            actualFlags = p->mountFlags;
        }
        else if(dynamic_cast<runtime::SiteMount*>(mountObject.get())) {
            const auto* p = dynamic_cast<runtime::SiteMount*>(mountObject.get());
            actualSource = p->source;
            actualDestination = p->destination;
            actualFlags = p->mountFlags;
        }
        else {
            FAIL("Mount object has unexpected dynamic type");
        }

        if(expectedSource) {
            CHECK(static_cast<bool>(actualSource));
            CHECK(*actualSource == *expectedSource);
        }

        if(expectedDestination) {
            CHECK(static_cast<bool>(actualDestination));
            CHECK(*actualDestination == *expectedDestination);
        }

        if(expectedFlags) {
            CHECK(static_cast<bool>(actualDestination));
            CHECK_EQUAL(*actualFlags, *expectedFlags);
        }
    }

private:
    std::string mountRequest;
    bool isSiteMount = false;
    boost::optional<std::string> expectedSource = {};
    boost::optional<std::string> expectedDestination = {};
    boost::optional<size_t> expectedFlags = {};

    bool isParseErrorExpected = false;
};

} // namespace
} // namespace
} // namespace
} // namespace

#endif
