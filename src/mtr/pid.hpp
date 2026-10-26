#pragma once

namespace mtr {

// Discrete PID in parallel form. Output units are whatever the caller assigns
// to the gains. This project uses normalized torque in [-1, 1] and rad/s.
//
//   e[k] = setpoint - measurement
//   P    = Kp * e[k]
//   I[k] = I[k-1] + Ki * e[k] * dt          (conditional integration)
//   d[k] = (measurement[k] - measurement[k-1]) / dt
//   d_f  = d_f + (dt / (Tf + dt)) * (d[k] - d_f)    if Tf > 0
//   D    = -Kd * d_f                         (derivative on measurement)
//   u    = clamp(P + I + D, u_min, u_max)
//
// Integration is skipped when the unsaturated output is beyond the limit it
// would be driven further past. Tf = 0 selects an unfiltered difference.
// The first update after reset forces D to 0 so a missing previous sample
// cannot produce a spike.
struct PidGains {
    float kp = 0.f;
    float ki = 0.f;
    float kd = 0.f;
    float tf_s = 0.f;
};

struct PidLimits {
    float minimum = -1.f;
    float maximum = 1.f;
};

class PidController {
public:
    bool set_gains(PidGains gains) noexcept;
    bool set_limits(PidLimits limits) noexcept;
    void reset() noexcept;

    // Returns false when gains have not been accepted, or dt / the signals
    // are not finite and positive (dt). On failure `output` is left unchanged.
    bool update(float setpoint, float measurement, float dt_s, float& output) noexcept;

    PidGains gains() const noexcept { return gains_; }
    PidLimits limits() const noexcept { return limits_; }
    float integrator() const noexcept { return integrator_; }
    float output() const noexcept { return output_; }
    bool configured() const noexcept { return configured_; }

private:
    static float clampf(float value, float minimum, float maximum) noexcept;

    PidGains gains_{};
    PidLimits limits_{-1.f, 1.f};
    float integrator_ = 0.f;
    float derivative_ = 0.f;
    float previous_measurement_ = 0.f;
    float output_ = 0.f;
    bool configured_ = false;
    bool has_measurement_ = false;
};

PidGains default_gains() noexcept;

}  // namespace mtr
