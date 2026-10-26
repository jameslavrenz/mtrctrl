#include "mtr/snapshot.hpp"

namespace mtr {
namespace {

void copy_words(volatile uint32_t* destination, const uint32_t* source, unsigned count) noexcept {
    for (unsigned index = 0; index < count; ++index) {
        destination[index] = source[index];
    }
}

void copy_words(uint32_t* destination, const volatile uint32_t* source, unsigned count) noexcept {
    for (unsigned index = 0; index < count; ++index) {
        destination[index] = source[index];
    }
}

}  // namespace

void SnapshotPublish::write(const ControlSnapshot& sample) noexcept {
    const auto* source = reinterpret_cast<const uint32_t*>(&sample);
    auto* destination = reinterpret_cast<volatile uint32_t*>(&data_);
    constexpr unsigned words = sizeof(ControlSnapshot) / 4;
    // Odd sequence means a write is in progress.
    sequence_ = sequence_ + 1u;
    copy_words(destination, source, words);
    sequence_ = sequence_ + 1u;
}

ControlSnapshot SnapshotPublish::read() const noexcept {
    constexpr unsigned words = sizeof(ControlSnapshot) / 4;
    ControlSnapshot sample{};
    for (;;) {
        const uint32_t begin = sequence_;
        if ((begin & 1u) != 0u) {
            continue;
        }
        copy_words(reinterpret_cast<uint32_t*>(&sample), reinterpret_cast<const volatile uint32_t*>(&data_),
                   words);
        const uint32_t end = sequence_;
        if (begin == end && (end & 1u) == 0u) {
            return sample;
        }
    }
}

}  // namespace mtr
