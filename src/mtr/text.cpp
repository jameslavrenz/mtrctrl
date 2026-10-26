#include "mtr/text.hpp"

#include <cmath>

namespace mtr {
namespace {

bool push(TextBuffer& buffer, char character) noexcept {
    if (buffer.data == nullptr || buffer.length + 1 >= buffer.capacity) {
        return false;
    }
    buffer.data[buffer.length++] = character;
    return true;
}

}  // namespace

bool text_append(TextBuffer& buffer, char character) noexcept { return push(buffer, character); }

bool text_append(TextBuffer& buffer, const char* text) noexcept {
    if (text == nullptr) {
        return false;
    }
    for (; *text != '\0'; ++text) {
        if (!push(buffer, *text)) {
            return false;
        }
    }
    return true;
}

bool text_append_u32(TextBuffer& buffer, uint32_t value) noexcept {
    char digits[10];
    unsigned count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    while (count > 0) {
        if (!push(buffer, digits[--count])) {
            return false;
        }
    }
    return true;
}

bool text_append_hex(TextBuffer& buffer, uint32_t value) noexcept {
    char digits[8];
    unsigned count = 0;
    do {
        const uint32_t nibble = value & 0xfu;
        digits[count++] = static_cast<char>(nibble < 10u ? '0' + nibble : 'a' + (nibble - 10u));
        value >>= 4;
    } while (value != 0u);
    while (count > 0) {
        if (!push(buffer, digits[--count])) {
            return false;
        }
    }
    return true;
}

bool text_append_i32(TextBuffer& buffer, int32_t value) noexcept {
    if (value < 0) {
        if (!push(buffer, '-')) {
            return false;
        }
        // INT32_MIN cannot be negated in int32_t.
        const uint32_t magnitude = static_cast<uint32_t>(-(value + 1)) + 1u;
        return text_append_u32(buffer, magnitude);
    }
    return text_append_u32(buffer, static_cast<uint32_t>(value));
}

bool text_append_f(TextBuffer& buffer, float value, int decimals) noexcept {
    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 6) {
        decimals = 6;
    }
    if (!std::isfinite(value)) {
        return text_append(buffer, std::isnan(value) ? "nan" : (value < 0.f ? "-inf" : "inf"));
    }
    if (value < 0.f) {
        if (!push(buffer, '-')) {
            return false;
        }
        value = -value;
    }

    float scale = 1.f;
    for (int place = 0; place < decimals; ++place) {
        scale *= 10.f;
    }
    const float scaled = std::floor(value * scale + 0.5f);
    auto whole_scaled = static_cast<uint32_t>(scaled);
    uint32_t factor = 1;
    for (int place = 0; place < decimals; ++place) {
        factor *= 10u;
    }
    const uint32_t integer_part = decimals == 0 ? whole_scaled : whole_scaled / factor;
    const uint32_t fraction = decimals == 0 ? 0u : whole_scaled % factor;
    if (!text_append_u32(buffer, integer_part)) {
        return false;
    }
    if (decimals == 0) {
        return true;
    }
    if (!push(buffer, '.')) {
        return false;
    }
    uint32_t place_value = factor / 10u;
    for (int place = 0; place < decimals; ++place) {
        const uint32_t digit = (fraction / place_value) % 10u;
        if (!push(buffer, static_cast<char>('0' + digit))) {
            return false;
        }
        place_value /= 10u;
    }
    return true;
}

bool text_terminate(TextBuffer& buffer) noexcept {
    if (buffer.data == nullptr || buffer.capacity == 0 || buffer.length >= buffer.capacity) {
        return false;
    }
    buffer.data[buffer.length] = '\0';
    return true;
}

}  // namespace mtr
