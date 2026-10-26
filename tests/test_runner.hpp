#pragma once

#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

struct TestFailure : std::exception {
    const char* what() const noexcept override { return message.c_str(); }
    std::string message;
};

inline void check_impl(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        TestFailure failure;
        char buffer[160];
        std::snprintf(buffer, sizeof buffer, "%s:%d: %s", file, line, expression);
        failure.message = buffer;
        throw failure;
    }
}

inline void check_near_impl(double actual, double expected, double tolerance, const char* file, int line) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        TestFailure failure;
        char buffer[200];
        std::snprintf(buffer, sizeof buffer, "%s:%d: %g != %g (tol %g)", file, line, actual, expected, tolerance);
        failure.message = buffer;
        throw failure;
    }
}

#define CHECK(condition) check_impl(static_cast<bool>(condition), #condition, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance) \
    check_near_impl(static_cast<double>(actual), static_cast<double>(expected), static_cast<double>(tolerance), \
                    __FILE__, __LINE__)

struct TestCase {
    const char* name;
    void (*function)();
};

int run_suite(const char* suite, const TestCase* tests, int count);
