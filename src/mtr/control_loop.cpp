#include "mtr/control_loop.hpp"

#include "mtr/units.hpp"

#include <cmath>

namespace mtr {

SpeedControlLoop::SpeedControlLoop(MotorPlant& plant) noexcept : plant_(plant) {
    (void)pid_.set_gains(default_gains());
    (void)pid_.set_limits(PidLimits{-1.f, 1.f});
}

bool SpeedControlLoop::set_gains(PidGains gains) noexcept { return pid_.set_gains(gains); }

bool SpeedControlLoop::set_kp(float kp) noexcept {
    PidGains gains = pid_.gains();
    gains.kp = kp;
    return pid_.set_gains(gains);
}

bool SpeedControlLoop::set_ki(float ki) noexcept {
    PidGains gains = pid_.gains();
    gains.ki = ki;
    return pid_.set_gains(gains);
}

bool SpeedControlLoop::set_kd(float kd) noexcept {
    PidGains gains = pid_.gains();
    gains.kd = kd;
    return pid_.set_gains(gains);
}

void SpeedControlLoop::set_setpoint_rpm(float rpm) noexcept {
    if (!std::isfinite(rpm) || rpm < -kMaxAbsSetpointRpm || rpm > kMaxAbsSetpointRpm) {
        return;
    }
    setpoint_rpm_ = rpm;
    setpoint_rad_s_ = rpm_to_rad_s(rpm);
}

void SpeedControlLoop::set_load_nm(float torque_nm) noexcept {
    if (!std::isfinite(torque_nm) || torque_nm < -kMaxAbsLoadNm || torque_nm > kMaxAbsLoadNm) {
        return;
    }
    plant_.set_load_torque_nm(torque_nm);
}

void SpeedControlLoop::reset_pid() noexcept { pid_.reset(); }

void SpeedControlLoop::note_plant(StepStatus status, FaultPolicy& fault) noexcept {
    if (status == StepStatus::Overspeed) {
        fault.on_overspeed();
    } else if (status == StepStatus::Numerical) {
        fault.on_numerical();
    }
}

void SpeedControlLoop::sample(float dt_s, FaultPolicy& fault) noexcept {
    const float speed = plant_.speed_rad_s();
    actual_rpm_ = rad_s_to_rpm(speed);
    error_rpm_ = setpoint_rpm_ - actual_rpm_;
    feedback_valid_ = plant_.feedback_valid();

    if (fault.state() == RunState::Faulted) {
        output_ = 0.f;
        plant_.set_torque_command(0.f);
        return;
    }

    if (fault.state() != RunState::Running) {
        output_ = 0.f;
        plant_.set_torque_command(0.f);
        pid_.reset();
        note_plant(plant_.step(dt_s), fault);
        return;
    }

    if (!plant_.feedback_valid() || !std::isfinite(speed)) {
        output_ = 0.f;
        plant_.set_torque_command(0.f);
        if (!plant_.feedback_valid()) {
            fault.on_feedback_invalid();
        } else {
            fault.on_numerical();
        }
        return;
    }

    float command = 0.f;
    if (!pid_.update(setpoint_rad_s_, speed, dt_s, command)) {
        output_ = 0.f;
        plant_.set_torque_command(0.f);
        fault.on_numerical();
        return;
    }

    output_ = command;
    plant_.set_torque_command(command);
    note_plant(plant_.step(dt_s), fault);
    if (fault.state() == RunState::Faulted) {
        output_ = 0.f;
        plant_.set_torque_command(0.f);
    }
}

}  // namespace mtr
