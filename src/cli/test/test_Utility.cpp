/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string>

#include "common/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Utility.hpp"
#include "test_utility/unittest_main_function.hpp"

using namespace sarus;

TEST_GROUP(CLIUtilityTestGroup) {
};

TEST(CLIUtilityTestGroup, isValidCLIInputImageID) {
    // valid image ids
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("image"));
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("image:tag"));
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("namespace/image:tag"));
    CHECK_TRUE(cli::utility::isValidCLIInputImageID("server/namespace/image:tag"));
    
    // invalid image ids
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../image"));

    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../image:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("image/..:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("image:../tag"));

    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../namespace/image:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("namespace/../image:tag"));

    CHECK_FALSE(cli::utility::isValidCLIInputImageID("../server/namespace/image:tag"));
    CHECK_FALSE(cli::utility::isValidCLIInputImageID("server/../image:tag"));
}

TEST(CLIUtilityTestGroup, parseImageID) {
    auto imageID = common::ImageID{};

    imageID = cli::utility::parseImageID("image");
    CHECK_EQUAL(imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"library"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"latest"});

    imageID = cli::utility::parseImageID("image:tag");
    CHECK_EQUAL(imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"library"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"tag"});

    imageID = cli::utility::parseImageID("namespace/image:tag");
    CHECK_EQUAL(imageID.server, std::string{"index.docker.io"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"namespace"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"tag"});

    imageID = cli::utility::parseImageID("server/namespace/image:tag");
    CHECK_EQUAL(imageID.server, std::string{"server"});
    CHECK_EQUAL(imageID.repositoryNamespace, std::string{"namespace"});
    CHECK_EQUAL(imageID.image, std::string{"image"});
    CHECK_EQUAL(imageID.tag, std::string{"tag"});
}

std::tuple<common::CLIArguments, common::CLIArguments> generateGroupedArguments(
        const common::CLIArguments& args,
        const boost::program_options::options_description& optionsDescription) {
    return cli::utility::groupOptionsAndPositionalArguments(args, optionsDescription);
}

TEST(CLIUtilityTestGroup, groupOptionsAndPositionalArguments) {
    // one argument
    {
        auto optionsDescription = boost::program_options::options_description();
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), true);
        CHECK_EQUAL(nameAndOptionArgs.argc(), 1);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
    }
    // one argument with options
    {
        auto optionsDescription = boost::program_options::options_description();
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0", "--option1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), true);
        CHECK_EQUAL(nameAndOptionArgs.argc(), 3);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"--option1"});
    }
    // two arguments
    {
        auto optionsDescription = boost::program_options::options_description();
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "arg1", "--option1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 1);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});

        CHECK_EQUAL(positionalArgs.argc(), 2);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
        CHECK_EQUAL(std::string{positionalArgs.argv()[1]}, std::string{"--option1"});
    }
    // multiple arguments separated by options
    {
        auto optionsDescription = boost::program_options::options_description();
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0", "arg1", "--option1", "arg2"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0"});

        CHECK_EQUAL(positionalArgs.argc(), 3);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
        CHECK_EQUAL(std::string{positionalArgs.argv()[1]}, std::string{"--option1"});
        CHECK_EQUAL(std::string{positionalArgs.argv()[2]}, std::string{"arg2"});
    }
    // long option without value
    {
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0", "Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // long option with adjacent value
    {
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0", "Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0=value0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0=value0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // long option with separated value not followed by an option
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0", boost::program_options::value<std::string>(&value0), "Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0", "value0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 3);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"value0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // long option with separated value followed by an option
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0", boost::program_options::value<std::string>(&value0), "Option 0")
                ("option1", "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0", "value0", "--option1", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 4);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"value0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[3]}, std::string{"--option1"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // long option which accepts value but value is not provided as last arg (corner case for processPossibleValueInNextToken())
    {
        auto value1 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0", "Option 0")
                ("option1", boost::program_options::value<std::string>(&value1), "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "--option0", "--option1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), true);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 3);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"--option0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"--option1"});
    }
    // short option without value
    {
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", "Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-o", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-o"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // short option with trailing characters (will result in CLI error, but now we just test grouping)
    {
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", "Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-ovalue", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-ovalue"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // short option with adjacent value
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", boost::program_options::value<std::string>(&value0),"Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-ovalue0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-ovalue0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // short option with separated value not followed by an option
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", boost::program_options::value<std::string>(&value0),"Option 0");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-o", "value0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 3);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-o"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"value0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // short option with separated value followed by an option
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", boost::program_options::value<std::string>(&value0),"Option 0")
                ("option1", "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-o", "value0", "--option1", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 4);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-o"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"value0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[3]}, std::string{"--option1"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // short option which accepts value but value is not provided as last arg (corner case for processPossibleValueInNextToken())
    {
        auto value1 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", "Option 0")
                ("option1,p", boost::program_options::value<std::string>(&value1), "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-o", "-p"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), true);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 3);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-o"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"-p"});
    }
    // sticky short options without value
    {
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", "Option 0")
                ("option1,p", "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-op", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-op"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // sticky short options with adjacent value
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0", boost::program_options::value<std::string>(&value0),"Option 0")
                ("option1,p", "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-povalue0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 2);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-povalue0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
    // sticky short options with separated value not followed by an option
    {
        auto value0 = std::string{};
        auto optionsDescription = boost::program_options::options_description();
        optionsDescription.add_options()
                ("option0,o", boost::program_options::value<std::string>(&value0),"Option 0")
                ("option1,p", "Option 1");
        common::CLIArguments nameAndOptionArgs, positionalArgs;
        std::tie(nameAndOptionArgs, positionalArgs) = generateGroupedArguments({"arg0", "-po", "value0", "arg1"}, optionsDescription);
        CHECK_EQUAL(positionalArgs.empty(), false);

        CHECK_EQUAL(nameAndOptionArgs.argc(), 3);
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[0]}, std::string{"arg0"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[1]}, std::string{"-po"});
        CHECK_EQUAL(std::string{nameAndOptionArgs.argv()[2]}, std::string{"value0"});

        CHECK_EQUAL(positionalArgs.argc(), 1);
        CHECK_EQUAL(std::string{positionalArgs.argv()[0]}, std::string{"arg1"});
    }
}

TEST(CLIUtilityTestGroup, validateNumberOfPositionalArguments) {
    // no positionals expected
    cli::utility::validateNumberOfPositionalArguments({}, 0, 0, "command");
    // 2 positionals expected
    cli::utility::validateNumberOfPositionalArguments({"arg0", "arg1"}, 2, 2, "command");
    // at least 1 positional expected
    cli::utility::validateNumberOfPositionalArguments({"arg0", "arg1", "arg2"}, 1, INT_MAX, "command");
    // too few arguments
    CHECK_THROWS(sarus::common::Error, cli::utility::validateNumberOfPositionalArguments({}, 1, 1, "command"));
    // too few arguments with no max
    CHECK_THROWS(sarus::common::Error, cli::utility::validateNumberOfPositionalArguments({"arg0"}, 2, INT_MAX, "command"));
    // too many arguments with 0 max
    CHECK_THROWS(sarus::common::Error, cli::utility::validateNumberOfPositionalArguments({"arg0", "arg1"}, 0, 0, "command"));
    // too many arguments with non-zero max
    CHECK_THROWS(sarus::common::Error, cli::utility::validateNumberOfPositionalArguments({"arg0", "arg1"}, 1, 1, "command"));
}

SARUS_UNITTEST_MAIN_FUNCTION();
