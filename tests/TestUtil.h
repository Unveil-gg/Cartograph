#pragma once

// Tiny check helper shared by the test executables.

#include <iostream>
#include <string>

namespace testutil {

inline int& Failures() {
    static int failed = 0;
    return failed;
}

/**
 * Record a failed check and print it.
 * @param cond Result of the check
 * @param message What failed
 */
inline void Expect(bool cond, const std::string& message) {
    if (!cond) {
        std::cerr << "FAIL: " << message << "\n";
        ++Failures();
    }
}

/**
 * Print the summary and return the process exit code.
 * @param name Test suite name
 * @return 0 when every check passed
 */
inline int Finish(const char* name) {
    if (Failures() != 0) {
        std::cerr << Failures() << " check(s) failed in " << name << "\n";
        return 1;
    }
    std::cout << name << " passed\n";
    return 0;
}

}  // namespace testutil
