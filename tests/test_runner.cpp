#include "test_runner.hpp"

#include <cstdio>

int run_suite(const char* suite, const TestCase* tests, int count) {
    int failed = 0;
    for (int index = 0; index < count; ++index) {
        try {
            tests[index].function();
            std::printf("ok   %s.%s\n", suite, tests[index].name);
        } catch (const TestFailure& failure) {
            std::printf("FAIL %s.%s: %s\n", suite, tests[index].name, failure.what());
            ++failed;
        } catch (const std::exception& exception) {
            std::printf("FAIL %s.%s: %s\n", suite, tests[index].name, exception.what());
            ++failed;
        }
    }
    return failed;
}
