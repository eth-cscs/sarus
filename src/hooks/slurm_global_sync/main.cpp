#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "Hook.hpp"


int main(int argc, char* argv[]) {
    try {
        auto hook = sarus::hooks::slurm_global_sync::Hook{};
        hook.performSynchronization();
    } catch(const sarus::common::Error& e) {
        sarus::common::Logger::getInstance().logErrorTrace(e, "SLURM global sync hook");
        exit(EXIT_FAILURE);
    }
    return 0;
}
