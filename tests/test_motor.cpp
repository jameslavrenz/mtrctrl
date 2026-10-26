#include "mtr/simulated_motor.hpp"
#include "mtr/units.hpp"
#include "test_runner.hpp"

namespace {

using mtr::PlantParameters;
using mtr::SimulatedMotor;
using mtr::StepStatus;

PlantParameters exact_plant() {
    PlantParameters parameters{};
    parameters.inertia_kg_m2 = 0.25f;
    parameters.friction_nm_s_per_rad = 0.f;
    parameters.torque_scale_nm = 1.f;
    parameters.overspeed_rad_s = 100.f;
    return parameters;
}

void euler_step_is_exact() {
    SimulatedMotor motor;
    CHECK(motor.configure(exact_plant()));
    motor.reset(0.f);
    motor.set_torque_command(1.f);
    CHECK(motor.step(0.25f) == StepStatus::Ok);
    CHECK_NEAR(motor.speed_rad_s(), 1.0, 1e-6);
    CHECK(motor.step(0.25f) == StepStatus::Ok);
    CHECK_NEAR(motor.speed_rad_s(), 2.0, 1e-6);
}

void friction_decelerates() {
    SimulatedMotor motor;
    auto parameters = exact_plant();
    parameters.friction_nm_s_per_rad = 1.f;
    CHECK(motor.configure(parameters));
    motor.reset(4.f);
    motor.set_torque_command(0.f);
    CHECK(motor.step(0.25f) == StepStatus::Ok);
    CHECK_NEAR(motor.speed_rad_s(), 0.0, 1e-5);
}

void load_reduces_speed() {
    SimulatedMotor motor;
    CHECK(motor.configure(exact_plant()));
    motor.reset(0.f);
    motor.set_torque_command(0.f);
    motor.set_load_torque_nm(2.f);
    CHECK(motor.step(0.25f) == StepStatus::Ok);
    CHECK_NEAR(motor.speed_rad_s(), -2.0, 1e-5);
}

void negative_command_reverses() {
    SimulatedMotor motor;
    CHECK(motor.configure(exact_plant()));
    motor.reset(0.f);
    motor.set_torque_command(-1.f);
    CHECK(motor.step(0.25f) == StepStatus::Ok);
    CHECK(motor.speed_rad_s() < 0.f);
}

void overspeed_does_not_commit() {
    SimulatedMotor motor;
    auto parameters = exact_plant();
    parameters.overspeed_rad_s = 0.5f;
    CHECK(motor.configure(parameters));
    motor.reset(0.f);
    motor.set_torque_command(1.f);
    CHECK(motor.step(0.25f) == StepStatus::Overspeed);
    CHECK_NEAR(motor.speed_rad_s(), 0.0, 1e-6);
}

void rejects_bad_parameters() {
    SimulatedMotor motor;
    const auto original = motor.parameters();
    auto bad = original;
    bad.inertia_kg_m2 = 0.f;
    CHECK(!motor.configure(bad));
    bad = original;
    bad.friction_nm_s_per_rad = -1.f;
    CHECK(!motor.configure(bad));
    CHECK_NEAR(motor.parameters().inertia_kg_m2, original.inertia_kg_m2, 1e-12);
}

void illustrative_full_scale_reaches_rated_speed() {
    SimulatedMotor motor;
    motor.reset(0.f);
    motor.set_torque_command(1.f);
    for (int step = 0; step < 500; ++step) {
        CHECK(motor.step(mtr::kControlDtS) == StepStatus::Ok);
    }
    CHECK_NEAR(mtr::rad_s_to_rpm(motor.speed_rad_s()), 1800.0, 1.0);
}

void open_loop_load_steady_state() {
    SimulatedMotor motor;
    const auto parameters = motor.parameters();
    constexpr float load = 0.02f;
    motor.set_load_torque_nm(load);
    motor.set_torque_command(1.f);
    for (int step = 0; step < 800; ++step) {
        CHECK(motor.step(mtr::kControlDtS) == StepStatus::Ok);
    }
    const float expected = (parameters.torque_scale_nm - load) / parameters.friction_nm_s_per_rad;
    CHECK_NEAR(motor.speed_rad_s(), expected, 0.5);
}

void feedback_flag_does_not_stop_integration() {
    SimulatedMotor motor;
    CHECK(motor.configure(exact_plant()));
    motor.set_feedback_valid(false);
    motor.set_torque_command(1.f);
    CHECK(!motor.feedback_valid());
    CHECK(motor.step(0.25f) == StepStatus::Ok);
    CHECK(motor.speed_rad_s() > 0.f);
}

const TestCase kTests[] = {
    {"euler_step_is_exact", euler_step_is_exact},
    {"friction_decelerates", friction_decelerates},
    {"load_reduces_speed", load_reduces_speed},
    {"negative_command_reverses", negative_command_reverses},
    {"overspeed_does_not_commit", overspeed_does_not_commit},
    {"rejects_bad_parameters", rejects_bad_parameters},
    {"illustrative_full_scale_reaches_rated_speed", illustrative_full_scale_reaches_rated_speed},
    {"open_loop_load_steady_state", open_loop_load_steady_state},
    {"feedback_flag_does_not_stop_integration", feedback_flag_does_not_stop_integration},
};

}  // namespace

int test_motor() {
    return run_suite("motor", kTests, static_cast<int>(sizeof kTests / sizeof kTests[0]));
}
