#pragma once

namespace mtr {

inline constexpr float kPi = 3.14159265358979323846f;

constexpr float rpm_to_rad_s(float rpm) noexcept {
    return rpm * (2.f * kPi / 60.f);
}

constexpr float rad_s_to_rpm(float rad_s) noexcept {
    return rad_s * (60.f / (2.f * kPi));
}

// Outer speed loop. The integer rate is the hardware-timer contract.
// kControlDtS is the float used by the discrete controller and the plant.
inline constexpr unsigned kControlHz = 100;
inline constexpr float kControlDtS = 0.01f;

inline constexpr float kRatedSpeedRpm = 1800.f;
inline constexpr float kRatedPowerW = 14.f;
inline constexpr float kMaxAbsSetpointRpm = kRatedSpeedRpm;
inline constexpr float kOverspeedRpm = 2500.f;
inline constexpr float kMaxAbsLoadNm = 0.05f;

inline constexpr float kMaxKp = 5.f;
inline constexpr float kMaxKi = 20.f;
inline constexpr float kMaxKd = 1.f;
inline constexpr float kMaxTfS = 1.f;

}  // namespace mtr
