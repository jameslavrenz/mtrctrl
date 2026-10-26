#pragma once

#include <cstddef>
#include <cstdint>

namespace mtr {

// Last control sample plus the STM32 timing counters that ride with it.
// Simulated fields and measured fields are both here; telemetry labels them.
// All members are 32-bit so a seqlock can copy the struct as words.
struct ControlSnapshot {
    uint32_t timestamp_ms;
    uint32_t sample_count;
    float setpoint_rpm;
    float actual_rpm;
    float error_rpm;
    float control_output;
    float load_torque_nm;
    uint32_t execution_us;
    uint32_t execution_max_us;
    uint32_t wake_latency_us;
    uint32_t wake_latency_max_us;
    int32_t release_jitter_us;
    int32_t release_jitter_abs_max_us;
    uint32_t response_us;
    uint32_t response_max_us;
    uint32_t deadline_misses;
    uint32_t exec_overruns;
    uint32_t queue_overflows;
    uint32_t fault_flags;
    uint32_t state;
    uint32_t feedback_valid;
};

// Single-writer, multi-reader snapshot. The writer never blocks. A reader
// retries if it observes a torn copy. On Cortex-M4 a 32-bit aligned store is
// atomic; the sequence counter detects a writer that preempts a reader.
class SnapshotPublish {
public:
    void write(const ControlSnapshot& sample) noexcept;
    ControlSnapshot read() const noexcept;

private:
    mutable volatile uint32_t sequence_ = 0;
    mutable ControlSnapshot data_{};
};

static_assert(sizeof(ControlSnapshot) % 4 == 0, "snapshot copy is word-wise");
static_assert(alignof(ControlSnapshot) >= 4, "snapshot must be word aligned");

}  // namespace mtr
