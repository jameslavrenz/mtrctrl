#include "mtr/pid.hpp"
#include "test_runner.hpp"

namespace {

using mtr::PidController;
using mtr::PidGains;
using mtr::PidLimits;

void proportional() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{2.f, 0.f, 0.f, 0.f}));
    CHECK(pid.set_limits(PidLimits{-100.f, 100.f}));
    float output = -1.f;
    CHECK(pid.update(4.f, 1.f, 0.25f, output));
    CHECK_NEAR(output, 6.0, 1e-6);
}

void integral_accumulates() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{0.f, 1.f, 0.f, 0.f}));
    CHECK(pid.set_limits(PidLimits{-10.f, 10.f}));
    float output = 0.f;
    for (int step = 0; step < 4; ++step) {
        CHECK(pid.update(1.f, 0.f, 0.25f, output));
    }
    CHECK_NEAR(output, 1.0, 1e-6);
    CHECK_NEAR(pid.integrator(), 1.0, 1e-6);
}

void anti_windup_holds_integrator() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{0.f, 1.f, 0.f, 0.f}));
    CHECK(pid.set_limits(PidLimits{-0.5f, 0.5f}));
    float output = 0.f;
    CHECK(pid.update(1.f, 0.f, 0.25f, output));
    CHECK_NEAR(output, 0.25, 1e-6);
    CHECK(pid.update(1.f, 0.f, 0.25f, output));
    CHECK_NEAR(output, 0.5, 1e-6);
    CHECK(pid.update(1.f, 0.f, 0.25f, output));
    CHECK_NEAR(output, 0.5, 1e-6);
    CHECK_NEAR(pid.integrator(), 0.5, 1e-6);
    CHECK(pid.update(1.f, 0.f, 0.25f, output));
    CHECK_NEAR(pid.integrator(), 0.5, 1e-6);
    // Error reverses, so integration is allowed back off the limit.
    CHECK(pid.update(1.f, 2.f, 0.25f, output));
    CHECK_NEAR(pid.integrator(), 0.25, 1e-6);
    CHECK_NEAR(output, 0.25, 1e-6);
}

void output_saturation() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{5.f, 0.f, 0.f, 0.f}));
    float output = 0.f;
    CHECK(pid.update(1.f, 0.f, 0.01f, output));
    CHECK_NEAR(output, 1.0, 1e-6);
    CHECK(pid.update(-1.f, 0.f, 0.01f, output));
    CHECK_NEAR(output, -1.0, 1e-6);
}

void derivative_ignores_setpoint_step() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{0.f, 0.f, 0.5f, 0.f}));
    CHECK(pid.set_limits(PidLimits{-100.f, 100.f}));
    float output = 1.f;
    CHECK(pid.update(0.f, 0.f, 0.25f, output));
    CHECK_NEAR(output, 0.0, 1e-6);
    CHECK(pid.update(10.f, 0.f, 0.25f, output));
    CHECK_NEAR(output, 0.0, 1e-6);
    CHECK(pid.update(10.f, 0.25f, 0.25f, output));
    CHECK_NEAR(output, -0.5, 1e-5);
}

void derivative_filter() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{0.f, 0.f, 1.f, 0.25f}));
    CHECK(pid.set_limits(PidLimits{-100.f, 100.f}));
    float output = 0.f;
    CHECK(pid.update(0.f, 0.f, 0.25f, output));
    CHECK(pid.update(0.f, 0.25f, 0.25f, output));
    CHECK_NEAR(output, -0.5, 1e-5);
    CHECK(pid.update(0.f, 0.5f, 0.25f, output));
    CHECK_NEAR(output, -0.75, 1e-5);
}

void reset_clears_state() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{0.f, 1.f, 0.f, 0.f}));
    CHECK(pid.set_limits(PidLimits{-10.f, 10.f}));
    float output = 0.f;
    CHECK(pid.update(1.f, 0.f, 0.25f, output));
    CHECK(pid.integrator() != 0.f);
    pid.reset();
    CHECK_NEAR(pid.integrator(), 0.0, 1e-6);
    CHECK(pid.update(0.f, 0.f, 0.25f, output));
    CHECK_NEAR(output, 0.0, 1e-6);
    CHECK_NEAR(pid.integrator(), 0.0, 1e-6);
}

void rejects_invalid_gains_and_keeps_previous() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{1.f, 0.f, 0.f, 0.f}));
    CHECK(!pid.set_gains(PidGains{-1.f, 0.f, 0.f, 0.f}));
    CHECK(!pid.set_gains(PidGains{0.f, 0.f, 0.f, -0.1f}));
    CHECK(!pid.set_gains(PidGains{0.f / 0.f, 0.f, 0.f, 0.f}));
    CHECK(!pid.set_limits(PidLimits{1.f, 1.f}));
    CHECK(!pid.set_limits(PidLimits{1.f, -1.f}));
    CHECK_NEAR(pid.gains().kp, 1.0, 1e-6);
    float output = 0.f;
    CHECK(pid.update(1.f, 0.f, 0.f, output) == false);
    CHECK(pid.update(1.f, 0.f, -1.f, output) == false);
}

void zero_setpoint_drives_negative() {
    PidController pid;
    CHECK(pid.set_gains(PidGains{0.5f, 0.f, 0.f, 0.f}));
    float output = 0.f;
    CHECK(pid.update(0.f, 2.f, 0.01f, output));
    CHECK_NEAR(output, -1.0, 1e-6);
}

const TestCase kTests[] = {
    {"proportional", proportional},
    {"integral_accumulates", integral_accumulates},
    {"anti_windup_holds_integrator", anti_windup_holds_integrator},
    {"output_saturation", output_saturation},
    {"derivative_ignores_setpoint_step", derivative_ignores_setpoint_step},
    {"derivative_filter", derivative_filter},
    {"reset_clears_state", reset_clears_state},
    {"rejects_invalid_gains_and_keeps_previous", rejects_invalid_gains_and_keeps_previous},
    {"zero_setpoint_drives_negative", zero_setpoint_drives_negative},
};

}  // namespace

int test_pid() {
    return run_suite("pid", kTests, static_cast<int>(sizeof kTests / sizeof kTests[0]));
}
