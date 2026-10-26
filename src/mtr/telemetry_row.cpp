#include "mtr/telemetry_row.hpp"

namespace mtr {

bool format_telemetry_column_comment(TextBuffer& buffer) noexcept {
    if (!text_append(buffer, "# ")) {
        return false;
    }
    for (unsigned index = 0; index < kTelemetryColumnCount; ++index) {
        if (index > 0u && !text_append(buffer, ',')) {
            return false;
        }
        if (!text_append(buffer, kTelemetryColumns[index])) {
            return false;
        }
    }
    return text_append(buffer, "\r\n");
}

bool format_telemetry_row(TextBuffer& buffer, const ControlSnapshot& snapshot, uint32_t cpu_percent) noexcept {
    if (!text_append(buffer, "tel,")) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.timestamp_ms) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_f(buffer, snapshot.setpoint_rpm, 2) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_f(buffer, snapshot.actual_rpm, 2) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_f(buffer, snapshot.error_rpm, 2) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_f(buffer, snapshot.control_output, 3) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_f(buffer, snapshot.load_torque_nm, 4) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.execution_us) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.wake_latency_us) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_i32(buffer, snapshot.release_jitter_us) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.response_us) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.deadline_misses) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.exec_overruns) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.queue_overflows) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.fault_flags) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.state) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, snapshot.feedback_valid) || !text_append(buffer, ',')) {
        return false;
    }
    if (!text_append_u32(buffer, cpu_percent)) {
        return false;
    }
    return text_append(buffer, "\r\n");
}

}  // namespace mtr
