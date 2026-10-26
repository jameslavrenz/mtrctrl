#pragma once

#include "mtr/fault_policy.hpp"
#include "mtr/motor_plant.hpp"
#include "mtr/pid.hpp"

namespace mtr {

// One control sample. The caller owns the release timing.
//
// Order, which removes the off-by-one between the controller and the plant:
//   1. Observe speed w[k] (this is the feedback the PID uses).
//   2. If Running and feedback is valid, compute u[k] and apply it.
//   3. Integrate the plant to w[k+1], except in Faulted where the state is held.
//   4. Stopped coasts with u = 0 and a reset controller.
//
// actual_rpm / error_rpm describe the observation at step 1, so the reported
// error is the error the PID acted on. The next sample observes the result.
class SpeedControlLoop {
public:
    explicit SpeedControlLoop(MotorPlant& plant) noexcept;

    bool set_gains(PidGains gains) noexcept;
    bool set_kp(float kp) noexcept;
    bool set_ki(float ki) noexcept;
    bool set_kd(float kd) noexcept;
    void set_setpoint_rpm(float rpm) noexcept;
    void set_load_nm(float torque_nm) noexcept;
    void reset_pid() noexcept;

    void sample(float dt_s, FaultPolicy& fault) noexcept;

    float setpoint_rpm() const noexcept { return setpoint_rpm_; }
    float actual_rpm() const noexcept { return actual_rpm_; }
    float error_rpm() const noexcept { return error_rpm_; }
    float output() const noexcept { return output_; }
    float load_nm() const noexcept { return plant_.load_torque_nm(); }
    bool feedback_valid() const noexcept { return feedback_valid_; }
    PidGains gains() const noexcept { return pid_.gains(); }

private:
    void note_plant(StepStatus status, FaultPolicy& fault) noexcept;

    MotorPlant& plant_;
    PidController pid_{};
    float setpoint_rpm_ = 0.f;
    float setpoint_rad_s_ = 0.f;
    float actual_rpm_ = 0.f;
    float error_rpm_ = 0.f;
    float output_ = 0.f;
    bool feedback_valid_ = true;
};

}  // namespace mtr
