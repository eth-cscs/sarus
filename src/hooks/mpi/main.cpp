#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "MpiHook.hpp"

int main(int argc, char* argv[]) {
    try {
        sarus::hooks::mpi::MpiHook{}.activateMpiSupport();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "MPI hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
