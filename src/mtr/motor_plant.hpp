#pragma once

namespace mtr {

// Minimal plant boundary shared by the speed loop.
// Phase 1 implements this with SimulatedMotor. Phase 2 can replace that
// implementation with encoder feedback and a torque/PWM command path.
// The speed loop depends on this surface, not on FreeRTOS or the HAL.
enum class StepStatus {
    Ok = 0,
    Overspeed,
    Numerical,
};

class MotorPlant {
public:
    virtual ~MotorPlant() = default;

    virtual void reset(float speed_rad_s) = 0;
    virtual void set_torque_command(float normalized) = 0;
    virtual void set_load_torque_nm(float torque_nm) = 0;
    virtual void set_feedback_valid(bool valid) = 0;
    virtual StepStatus step(float dt_s) = 0;
    virtual float speed_rad_s() const = 0;
    virtual float load_torque_nm() const = 0;
    virtual bool feedback_valid() const = 0;
};

}  // namespace mtr
