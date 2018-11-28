#ifndef cli_HelpMessage_hpp
#define cli_HelpMessage_hpp

#include <ostream>
#include <memory>
#include <string>

#include <boost/program_options.hpp>


namespace sarus {
namespace cli {

class HelpMessage {
    friend std::ostream& operator<<(std::ostream&, const HelpMessage&);

public:
    HelpMessage();
    HelpMessage(const HelpMessage&);
    HelpMessage& setUsage(const std::string&);
    HelpMessage& setDescription(const std::string&);
    HelpMessage& setOptionsDescription(const boost::program_options::options_description&);

private:
    std::string usage;
    std::string description;
    std::unique_ptr<boost::program_options::options_description> optionsDescription;
};

std::ostream& operator<<(std::ostream&, const HelpMessage&);

}
}

#endif