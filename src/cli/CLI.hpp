#ifndef cli_CLI_hpp
#define cli_CLI_hpp

#include <memory>
#include <deque>

#include <boost/program_options.hpp>
#include <boost/format.hpp>

#include "common/Logger.hpp"
#include "cli/Utility.hpp"
#include "common/Config.hpp"
#include "cli/Command.hpp"
#include "common/CLIArguments.hpp"


namespace sarus {
namespace cli {

class CLI {
public:
    CLI();
    std::unique_ptr<cli::Command> parseCommandLine(const common::CLIArguments&, common::Config&) const;

// these methods are public for test purpose
public:
    std::deque<common::CLIArguments> groupArgumentsAndCorrespondingOptions(const common::CLIArguments&) const;
    const boost::program_options::options_description& getOptionsDescription() const;

private:
    std::unique_ptr<cli::Command> parseCommandHelpOfCommand(const std::deque<common::CLIArguments>&) const;

private:
    boost::program_options::options_description optionsDescription{"Options"};
};

}
}

#endif
