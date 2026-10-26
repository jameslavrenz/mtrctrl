#include "mtr/pid.hpp"

#include "mtr/units.hpp"

#include <cmath>

namespace mtr {

float PidController::clampf(float value, float minimum, float maximum) noexcept {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

bool PidController::set_gains(PidGains gains) noexcept {
    if (!std::isfinite(gains.kp) || !std::isfinite(gains.ki) || !std::isfinite(gains.kd) ||
        !std::isfinite(gains.tf_s)) {
        return false;
    }
    if (gains.kp < 0.f || gains.ki < 0.f || gains.kd < 0.f || gains.tf_s < 0.f) {
        return false;
    }
    if (gains.kp > kMaxKp || gains.ki > kMaxKi || gains.kd > kMaxKd || gains.tf_s > kMaxTfS) {
        return false;
    }
    gains_ = gains;
    configured_ = true;
    return true;
}

bool PidController::set_limits(PidLimits limits) noexcept {
    if (!std::isfinite(limits.minimum) || !std::isfinite(limits.maximum)) {
        return false;
    }
    if (!(limits.minimum < limits.maximum)) {
        return false;
    }
    limits_ = limits;
    return true;
}

void PidController::reset() noexcept {
    integrator_ = 0.f;
    derivative_ = 0.f;
    previous_measurement_ = 0.f;
    output_ = 0.f;
    has_measurement_ = false;
}

bool PidController::update(float setpoint, float measurement, float dt_s, float& output) noexcept {
    if (!configured_ || !(dt_s > 0.f) || !std::isfinite(dt_s) || !std::isfinite(setpoint) ||
        !std::isfinite(measurement)) {
        return false;
    }

    const float error = setpoint - measurement;
    const float proportional = gains_.kp * error;

    if (!has_measurement_) {
        derivative_ = 0.f;
        has_measurement_ = true;
    } else {
        const float rate = (measurement - previous_measurement_) / dt_s;
        if (gains_.tf_s > 0.f) {
            const float alpha = dt_s / (gains_.tf_s + dt_s);
            derivative_ += alpha * (rate - derivative_);
        } else {
            derivative_ = rate;
        }
    }
    previous_measurement_ = measurement;
    const float derivative_term = -gains_.kd * derivative_;

    const float candidate = integrator_ + gains_.ki * error * dt_s;
    const float unsaturated = proportional + candidate + derivative_term;
    const bool driving_high = unsaturated > limits_.maximum && error > 0.f;
    const bool driving_low = unsaturated < limits_.minimum && error < 0.f;
    if (!driving_high && !driving_low) {
        integrator_ = candidate;
    }

    output_ = clampf(proportional + integrator_ + derivative_term, limits_.minimum, limits_.maximum);
    output = output_;
    return true;
}

PidGains default_gains() noexcept {
    // Tuned on the illustrative plant in simulated_motor.hpp for a 1000 RPM
    // step with a few percent overshoot and recovery from a 0.02 N·m load.
    // See docs/control-design.md. These are not claimed as optimal.
    return PidGains{0.012f, 0.06f, 0.0005f, 0.03f};
}

}  // namespace mtr
