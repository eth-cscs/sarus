#include <string>
#include <boost/format.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "SshHook.hpp"

int main(int argc, char* argv[]) {
    try {
        if(argc != 2) {
            SARUS_THROW_ERROR("Failed to execute SSH hook."
                                " Bad number of CLI arguments."
                                " Expected exactly one argument.");
        }

        if(argv[1] == std::string{"keygen"}) {
            sarus::hooks::ssh::SshHook{}.generateSshKeys();
        }
        else if(argv[1] == std::string{"check-localrepository-has-sshkeys"}) {
            sarus::hooks::ssh::SshHook{}.checkLocalRepositoryHasSshKeys();
        }
        else if(argv[1] == std::string("start-sshd")) {
            sarus::hooks::ssh::SshHook{}.startSshd();
        }
        else {
            auto message = boost::format("Failed to execute SSH hook. CLI argument %s is not supported.")
                % argv[1];
            SARUS_THROW_ERROR(message.str());
        }
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "SSH hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
