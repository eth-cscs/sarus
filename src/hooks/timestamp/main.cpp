#include "TimestampHook.hpp"
#include "common/Error.hpp"
#include "common/Logger.hpp"

int main(int argc, char* argv[]) {
    try {
        std::string message;
        if (argc >= 2) {
            message = std::string("");
        }
        else {
            message = std::string(argv[1]);
        }
        sarus::hooks::timestamp::TimestampHook{message}.activate();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "Timestamp hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
