#pragma once

#include <cstdint>

namespace mtr {

// Five measured control-body spins. A fault latch does not cancel spins that
// are already armed: the wake that trips the deadline is often a short sample,
// and a short sample clears the overrun streak before kFaultOverrun can latch.
// stop and reset_faults call cancel().
class DeadlineInjection {
public:
    static constexpr uint32_t kSpins = 5;

    void arm_if_running(bool running) noexcept {
        if (running) {
            left_ = kSpins;
        }
    }

    void cancel() noexcept { left_ = 0; }

    bool armed() const noexcept { return left_ > 0u; }

    uint32_t left() const noexcept { return left_; }

    // One control sample. Fault state is not an input.
    bool consume() noexcept {
        if (left_ == 0u) {
            return false;
        }
        --left_;
        return true;
    }

private:
    uint32_t left_ = 0;
};

}  // namespace mtr
