#include "test_runner.hpp"

int test_pid();
int test_motor();
int test_loop();
int test_support();
int test_telemetry();

int main() {
    int failed = 0;
    failed += test_pid();
    failed += test_motor();
    failed += test_loop();
    failed += test_support();
    failed += test_telemetry();
    return failed == 0 ? 0 : 1;
}
