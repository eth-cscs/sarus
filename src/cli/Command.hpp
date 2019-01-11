#ifndef cli_command_hpp
#define cli_command_hpp

#include "common/Config.hpp"

namespace sarus {
namespace cli {

class Command {
public:
    virtual void execute() = 0;
    virtual bool requiresRootPrivileges() const = 0;
    virtual std::string getBriefDescription() const = 0;
    virtual void printHelpMessage() const = 0;
};

}
}

#endif