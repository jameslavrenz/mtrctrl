#pragma once

#include "mtr/snapshot.hpp"
#include "mtr/text.hpp"

namespace mtr {

// Order is the on-wire contract. tools/telemetry_format.py reads these names.
inline constexpr const char* kTelemetryColumns[] = {
    "kind",
    "timestamp_ms",
    "setpoint_rpm",
    "actual_rpm",
    "error_rpm",
    "control_output",
    "load_torque_nm",
    "execution_us",
    "wake_latency_us",
    "release_jitter_us",
    "response_us",
    "deadline_misses",
    "exec_overruns",
    "queue_overflows",
    "fault_flags",
    "state",
    "feedback_valid",
    "cpu_percent",
};

inline constexpr unsigned kTelemetryColumnCount = sizeof kTelemetryColumns / sizeof kTelemetryColumns[0];

bool format_telemetry_row(TextBuffer& buffer, const ControlSnapshot& snapshot, uint32_t cpu_percent) noexcept;
bool format_telemetry_column_comment(TextBuffer& buffer) noexcept;

}  // namespace mtr
