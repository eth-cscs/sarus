#ifndef cli_CommandObjectsFactory_hpp
#define cli_CommandObjectsFactory_hpp

#include <string>
#include <vector>
#include <deque>

#include "common/Config.hpp"
#include "common/CLIArguments.hpp"
#include "cli/Command.hpp"


namespace sarus {
namespace cli {

class CommandObjectsFactory {
public:
    CommandObjectsFactory();

    template<class CommandType>
    void addCommand(const std::string& commandName) {
        map[commandName] = []() {
            return std::unique_ptr<cli::Command>{new CommandType{}};
        };
        mapWithArguments[commandName] = [](const std::deque<common::CLIArguments>& commandArgsGroups, const common::Config& config) {
            return std::unique_ptr<cli::Command>{new CommandType{commandArgsGroups, config}};
        };
    }

    bool isValidCommandName(const std::string& commandName) const;
    std::vector<std::string> getCommandNames() const;
    std::unique_ptr<cli::Command> makeCommandObject(const std::string& commandName) const;
    std::unique_ptr<cli::Command> makeCommandObject(const std::string& commandName,
                                                    const std::deque<common::CLIArguments>& commandArgsGroups,
                                                    const common::Config& config) const;
    std::unique_ptr<cli::Command> makeCommandObjectHelpOfCommand(const std::string& commandName) const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<cli::Command>()>> map;
    std::unordered_map<std::string, std::function<std::unique_ptr<cli::Command>(const std::deque<common::CLIArguments>&, const common::Config&)>> mapWithArguments;
};

}
}

#endif
