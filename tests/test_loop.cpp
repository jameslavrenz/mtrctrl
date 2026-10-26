#include "mtr/control_loop.hpp"
#include "mtr/deadline_injection.hpp"
#include "mtr/fault_policy.hpp"
#include "mtr/simulated_motor.hpp"
#include "mtr/units.hpp"
#include "test_runner.hpp"

namespace {

void start_stop_and_fault_gate() {
    mtr::FaultPolicy fault;
    CHECK(fault.state() == mtr::RunState::Stopped);
    CHECK(fault.start());
    CHECK(fault.state() == mtr::RunState::Running);
    fault.stop();
    CHECK(fault.state() == mtr::RunState::Stopped);
    fault.on_feedback_invalid();
    CHECK(fault.state() == mtr::RunState::Faulted);
    CHECK((fault.flags() & mtr::kFaultFeedback) != 0);
    CHECK(!fault.start());
    fault.stop();
    CHECK(fault.state() == mtr::RunState::Faulted);
    fault.reset_faults();
    CHECK(fault.state() == mtr::RunState::Stopped);
    CHECK(fault.flags() == 0);
    CHECK(fault.start());
}

void deadline_threshold() {
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    fault.on_deadline_misses(1);
    fault.on_deadline_misses(1);
    CHECK(fault.state() == mtr::RunState::Running);
    fault.on_deadline_misses(1);
    CHECK(fault.state() == mtr::RunState::Faulted);
    CHECK(fault.deadline_misses() == 3);
}

void overrun_is_consecutive() {
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    fault.on_exec_overrun(true);
    fault.on_exec_overrun(true);
    fault.on_exec_overrun(false);
    fault.on_exec_overrun(true);
    CHECK(fault.state() == mtr::RunState::Running);
    fault.on_exec_overrun(true);
    fault.on_exec_overrun(true);
    CHECK(fault.state() == mtr::RunState::Faulted);
    CHECK((fault.flags() & mtr::kFaultOverrun) != 0);
}

void queue_threshold() {
    mtr::FaultPolicy fault;
    fault.on_queue_overflows(7);
    CHECK(fault.state() == mtr::RunState::Stopped);
    fault.on_queue_overflows(1);
    CHECK(fault.state() == mtr::RunState::Faulted);
}

void closed_loop_step_and_load() {
    mtr::SimulatedMotor motor;
    mtr::SpeedControlLoop loop(motor);
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    loop.set_setpoint_rpm(1000.f);
    float peak = 0.f;
    for (int step = 0; step < 300; ++step) {
        loop.sample(mtr::kControlDtS, fault);
        CHECK(fault.state() == mtr::RunState::Running);
        if (loop.actual_rpm() > peak) {
            peak = loop.actual_rpm();
        }
    }
    CHECK_NEAR(loop.actual_rpm(), 1000.0, 20.0);
    CHECK(peak < 1200.f);

    loop.set_load_nm(0.02f);
    for (int step = 0; step < 300; ++step) {
        loop.sample(mtr::kControlDtS, fault);
        CHECK(fault.state() == mtr::RunState::Running);
    }
    CHECK_NEAR(loop.actual_rpm(), 1000.0, 30.0);
}

void coast_after_stop() {
    mtr::SimulatedMotor motor;
    mtr::SpeedControlLoop loop(motor);
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    loop.set_setpoint_rpm(1000.f);
    for (int step = 0; step < 300; ++step) {
        loop.sample(mtr::kControlDtS, fault);
    }
    const float before = motor.speed_rad_s();
    fault.stop();
    for (int step = 0; step < 50; ++step) {
        loop.sample(mtr::kControlDtS, fault);
    }
    CHECK(motor.speed_rad_s() < before);
    CHECK_NEAR(loop.output(), 0.0, 1e-6);
}

void feedback_loss_trips_and_holds() {
    mtr::SimulatedMotor motor;
    mtr::SpeedControlLoop loop(motor);
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    loop.set_setpoint_rpm(500.f);
    for (int step = 0; step < 50; ++step) {
        loop.sample(mtr::kControlDtS, fault);
    }
    const float held = motor.speed_rad_s();
    motor.set_feedback_valid(false);
    loop.sample(mtr::kControlDtS, fault);
    CHECK(fault.state() == mtr::RunState::Faulted);
    CHECK_NEAR(loop.output(), 0.0, 1e-6);
    loop.sample(mtr::kControlDtS, fault);
    CHECK_NEAR(motor.speed_rad_s(), held, 1e-5);
}

void zero_gains_do_not_track() {
    mtr::SimulatedMotor motor;
    mtr::SpeedControlLoop loop(motor);
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    CHECK(loop.set_gains(mtr::PidGains{0.f, 0.f, 0.f, 0.f}));
    loop.set_setpoint_rpm(1000.f);
    for (int step = 0; step < 100; ++step) {
        loop.sample(mtr::kControlDtS, fault);
    }
    CHECK(loop.actual_rpm() < 1.f);
}

void overspeed_trips() {
    mtr::SimulatedMotor motor;
    auto parameters = motor.parameters();
    parameters.inertia_kg_m2 = 1.0e-6f;
    parameters.overspeed_rad_s = mtr::rpm_to_rad_s(100.f);
    CHECK(motor.configure(parameters));
    mtr::SpeedControlLoop loop(motor);
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    loop.set_setpoint_rpm(1800.f);
    bool tripped = false;
    for (int step = 0; step < 20; ++step) {
        loop.sample(mtr::kControlDtS, fault);
        if (fault.state() == mtr::RunState::Faulted) {
            tripped = true;
            break;
        }
    }
    CHECK(tripped);
    CHECK((fault.flags() & mtr::kFaultOverspeed) != 0);
}

void injection_continues_after_deadline_latch() {
    mtr::DeadlineInjection injection;
    mtr::FaultPolicy fault;
    CHECK(fault.start());
    injection.arm_if_running(false);
    CHECK(!injection.armed());
    injection.arm_if_running(fault.state() == mtr::RunState::Running);
    CHECK(injection.left() == mtr::DeadlineInjection::kSpins);

    int burns = 0;
    for (int step = 0; step < 2; ++step) {
        CHECK(injection.consume());
        fault.on_exec_overrun(true);
        ++burns;
    }
    fault.on_deadline_misses(mtr::FaultPolicy::kDeadlineTrip);
    CHECK(fault.state() == mtr::RunState::Faulted);
    CHECK((fault.flags() & mtr::kFaultDeadline) != 0);
    CHECK((fault.flags() & mtr::kFaultOverrun) == 0);

    while (injection.armed()) {
        CHECK(injection.consume());
        fault.on_exec_overrun(true);
        ++burns;
    }
    CHECK(burns == static_cast<int>(mtr::DeadlineInjection::kSpins));
    CHECK(!injection.consume());
    CHECK((fault.flags() & mtr::kFaultOverrun) != 0);
    fault.on_exec_overrun(false);
    CHECK((fault.flags() & mtr::kFaultOverrun) != 0);
    CHECK((fault.flags() & mtr::kFaultDeadline) != 0);
}

void injection_cancel_on_stop_or_reset() {
    mtr::DeadlineInjection injection;
    injection.arm_if_running(true);
    CHECK(injection.consume());
    injection.cancel();
    CHECK(!injection.consume());
    CHECK(!injection.armed());
}

const TestCase kTests[] = {
    {"start_stop_and_fault_gate", start_stop_and_fault_gate},
    {"deadline_threshold", deadline_threshold},
    {"overrun_is_consecutive", overrun_is_consecutive},
    {"queue_threshold", queue_threshold},
    {"closed_loop_step_and_load", closed_loop_step_and_load},
    {"coast_after_stop", coast_after_stop},
    {"feedback_loss_trips_and_holds", feedback_loss_trips_and_holds},
    {"zero_gains_do_not_track", zero_gains_do_not_track},
    {"overspeed_trips", overspeed_trips},
    {"injection_continues_after_deadline_latch", injection_continues_after_deadline_latch},
    {"injection_cancel_on_stop_or_reset", injection_cancel_on_stop_or_reset},
};

}  // namespace

int test_loop() {
    return run_suite("loop", kTests, static_cast<int>(sizeof kTests / sizeof kTests[0]));
}
