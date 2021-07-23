/*
 * Sarus
 *
 * Copyright (c) 2018-2021, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_test_utilities_unittest_main_function_hpp
#define sarus_test_utilities_unittest_main_function_hpp

#include "common/Error.hpp"
#include "common/Logger.hpp"

// WATCH OUT!
// boost libraries must be included before CppUTest, so in order to be
// on the safe side include this file as the last header file in the test code
#include <CppUTest/CommandLineTestRunner.h>


#define SARUS_UNITTEST_MAIN_FUNCTION() \
int main(int argc, char **argv) { \
    /* disable cpputest's leak detection to avoid false positives, e.g. because of static objects */ \
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads(); \
    try { \
        return CommandLineTestRunner::RunAllTests(argc, argv); \
    } \
    catch(const sarus::common::Error& e) { \
        sarus::common::Logger::getInstance().logErrorTrace(e, "test"); \
        throw; \
    } \
}

#endif
