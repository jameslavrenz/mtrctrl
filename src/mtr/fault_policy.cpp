#include "mtr/fault_policy.hpp"

namespace mtr {

void FaultPolicy::trip(uint32_t bit) noexcept {
    flags_ |= bit;
    state_ = RunState::Faulted;
}

bool FaultPolicy::start() noexcept {
    if (state_ == RunState::Faulted) {
        return false;
    }
    state_ = RunState::Running;
    return true;
}

void FaultPolicy::stop() noexcept {
    if (state_ != RunState::Faulted) {
        state_ = RunState::Stopped;
    }
}

void FaultPolicy::reset_faults() noexcept {
    flags_ = 0;
    deadline_misses_ = 0;
    consecutive_overruns_ = 0;
    exec_overruns_ = 0;
    queue_overflows_ = 0;
    if (state_ == RunState::Faulted) {
        state_ = RunState::Stopped;
    }
}

void FaultPolicy::on_deadline_misses(uint32_t count) noexcept {
    if (count == 0) {
        return;
    }
    deadline_misses_ += count;
    if (deadline_misses_ >= kDeadlineTrip) {
        trip(kFaultDeadline);
    }
}

void FaultPolicy::on_exec_overrun(bool overrun) noexcept {
    if (!overrun) {
        consecutive_overruns_ = 0;
        return;
    }
    ++exec_overruns_;
    ++consecutive_overruns_;
    if (consecutive_overruns_ >= kOverrunTrip) {
        trip(kFaultOverrun);
    }
}

void FaultPolicy::on_feedback_invalid() noexcept { trip(kFaultFeedback); }

void FaultPolicy::on_overspeed() noexcept { trip(kFaultOverspeed); }

void FaultPolicy::on_numerical() noexcept { trip(kFaultNumerical); }

void FaultPolicy::on_queue_overflows(uint32_t count) noexcept {
    if (count == 0) {
        return;
    }
    queue_overflows_ += count;
    if (queue_overflows_ >= kQueueTrip) {
        trip(kFaultQueue);
    }
}

}  // namespace mtr
