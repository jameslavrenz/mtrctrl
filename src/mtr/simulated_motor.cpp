#include "mtr/simulated_motor.hpp"

#include "mtr/units.hpp"

#include <cmath>

namespace mtr {
namespace {

float clampf(float value, float minimum, float maximum) noexcept {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

bool parameters_ok(const PlantParameters& parameters) noexcept {
    return std::isfinite(parameters.inertia_kg_m2) && parameters.inertia_kg_m2 > 0.f &&
           std::isfinite(parameters.friction_nm_s_per_rad) && parameters.friction_nm_s_per_rad >= 0.f &&
           std::isfinite(parameters.torque_scale_nm) && parameters.torque_scale_nm > 0.f &&
           std::isfinite(parameters.overspeed_rad_s) && parameters.overspeed_rad_s > 0.f;
}

}  // namespace

PlantParameters fit1035_illustrative_plant() noexcept {
    const float rated_speed = rpm_to_rad_s(kRatedSpeedRpm);
    const float rated_torque = kRatedPowerW / rated_speed;
    PlantParameters parameters{};
    parameters.inertia_kg_m2 = 8.0e-5f;
    parameters.friction_nm_s_per_rad = rated_torque / rated_speed;
    parameters.torque_scale_nm = rated_torque;
    parameters.overspeed_rad_s = rpm_to_rad_s(kOverspeedRpm);
    return parameters;
}

SimulatedMotor::SimulatedMotor() noexcept {
    (void)configure(fit1035_illustrative_plant());
}

bool SimulatedMotor::configure(PlantParameters parameters) noexcept {
    if (!parameters_ok(parameters)) {
        return false;
    }
    parameters_ = parameters;
    return true;
}

void SimulatedMotor::reset(float speed_rad_s) {
    speed_rad_s_ = std::isfinite(speed_rad_s) ? speed_rad_s : 0.f;
    command_ = 0.f;
    command_finite_ = true;
}

void SimulatedMotor::set_torque_command(float normalized) {
    if (!std::isfinite(normalized)) {
        command_ = 0.f;
        command_finite_ = false;
        return;
    }
    command_finite_ = true;
    command_ = clampf(normalized, -1.f, 1.f);
}

void SimulatedMotor::set_load_torque_nm(float torque_nm) {
    load_nm_ = std::isfinite(torque_nm) ? torque_nm : 0.f;
}

void SimulatedMotor::set_feedback_valid(bool valid) { feedback_valid_ = valid; }

StepStatus SimulatedMotor::step(float dt_s) {
    if (!(dt_s > 0.f) || !std::isfinite(dt_s) || !command_finite_) {
        return StepStatus::Numerical;
    }
    const float torque = command_ * parameters_.torque_scale_nm;
    const float accel =
        (torque - parameters_.friction_nm_s_per_rad * speed_rad_s_ - load_nm_) / parameters_.inertia_kg_m2;
    const float next = speed_rad_s_ + dt_s * accel;
    if (!std::isfinite(next)) {
        return StepStatus::Numerical;
    }
    if (std::fabs(next) > parameters_.overspeed_rad_s) {
        // Reject the step. Leaving the previous speed in place keeps a
        // diverging sample from becoming the next controller measurement.
        return StepStatus::Overspeed;
    }
    speed_rad_s_ = next;
    return StepStatus::Ok;
}

}  // namespace mtr
