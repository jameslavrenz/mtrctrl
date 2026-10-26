#pragma once

#include <cstdint>

namespace mtr {

enum class RunState : uint8_t {
    Stopped = 0,
    Running = 1,
    Faulted = 2,
};

// Bits are stable. Telemetry prints the raw mask.
inline constexpr uint32_t kFaultDeadline = 1u << 0;
inline constexpr uint32_t kFaultFeedback = 1u << 1;
inline constexpr uint32_t kFaultOverspeed = 1u << 2;
inline constexpr uint32_t kFaultNumerical = 1u << 3;
inline constexpr uint32_t kFaultQueue = 1u << 4;
inline constexpr uint32_t kFaultOverrun = 1u << 5;

// Scheduling and numeric policy for the speed loop. No FreeRTOS calls.
// The control task is the only writer. A fault forces RunState::Faulted until
// reset_faults(), which returns to Stopped and does not resume the loop.
class FaultPolicy {
public:
    static constexpr uint32_t kDeadlineTrip = 3;
    static constexpr uint32_t kOverrunTrip = 3;
    static constexpr uint32_t kQueueTrip = 8;

    RunState state() const noexcept { return state_; }
    uint32_t flags() const noexcept { return flags_; }
    uint32_t deadline_misses() const noexcept { return deadline_misses_; }
    uint32_t exec_overruns() const noexcept { return exec_overruns_; }
    uint32_t queue_overflows() const noexcept { return queue_overflows_; }

    bool start() noexcept;
    void stop() noexcept;
    void reset_faults() noexcept;

    void on_deadline_misses(uint32_t count) noexcept;
    void on_exec_overrun(bool overrun) noexcept;
    void on_feedback_invalid() noexcept;
    void on_overspeed() noexcept;
    void on_numerical() noexcept;
    void on_queue_overflows(uint32_t count) noexcept;

private:
    void trip(uint32_t bit) noexcept;

    RunState state_ = RunState::Stopped;
    uint32_t flags_ = 0;
    uint32_t deadline_misses_ = 0;
    uint32_t consecutive_overruns_ = 0;
    uint32_t exec_overruns_ = 0;
    uint32_t queue_overflows_ = 0;
};

}  // namespace mtr
