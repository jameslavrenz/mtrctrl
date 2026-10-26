#pragma once

#include "mtr/motor_plant.hpp"

namespace mtr {

// Mechanical model only:
//   J * dw/dt = Tm - b * w - Tl
//   Tm = saturate(u, -1, 1) * torque_scale
// Integrated with forward Euler. Electrical dynamics are not modeled.
struct PlantParameters {
    float inertia_kg_m2 = 0.f;
    float friction_nm_s_per_rad = 0.f;
    float torque_scale_nm = 0.f;
    float overspeed_rad_s = 0.f;
};

// torque_scale and friction are derived from the FIT1035's published 14 W and
// 1800 RPM (T = P / w, b = T / w so full-scale torque balances at rated speed).
// inertia is illustrative: the manufacturer does not publish rotor inertia, and
// the value includes a notional load so the 100 Hz loop has visible dynamics.
// This is not a validated model of the physical motor.
PlantParameters fit1035_illustrative_plant() noexcept;

class SimulatedMotor final : public MotorPlant {
public:
    SimulatedMotor() noexcept;

    bool configure(PlantParameters parameters) noexcept;
    PlantParameters parameters() const noexcept { return parameters_; }

    void reset(float speed_rad_s) override;
    void set_torque_command(float normalized) override;
    void set_load_torque_nm(float torque_nm) override;
    void set_feedback_valid(bool valid) override;
    StepStatus step(float dt_s) override;
    float speed_rad_s() const override { return speed_rad_s_; }
    float load_torque_nm() const override { return load_nm_; }
    bool feedback_valid() const override { return feedback_valid_; }

private:
    PlantParameters parameters_{};
    float speed_rad_s_ = 0.f;
    float command_ = 0.f;
    float load_nm_ = 0.f;
    bool feedback_valid_ = true;
    bool command_finite_ = true;
};

}  // namespace mtr
