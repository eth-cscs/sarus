#include "TimestampHook.hpp"
#include "common/Error.hpp"
#include "common/Logger.hpp"

int main(int argc, char* argv[]) {
    try {
        sarus::hooks::timestamp::TimestampHook{}.activate();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "Timestamp hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
