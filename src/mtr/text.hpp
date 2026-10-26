#pragma once

#include <cstddef>
#include <cstdint>

namespace mtr {

struct TextBuffer {
    char* data = nullptr;
    std::size_t capacity = 0;
    std::size_t length = 0;
};

inline void text_reset(TextBuffer& buffer) noexcept { buffer.length = 0; }

bool text_append(TextBuffer& buffer, char character) noexcept;
bool text_append(TextBuffer& buffer, const char* text) noexcept;
bool text_append_u32(TextBuffer& buffer, uint32_t value) noexcept;
bool text_append_i32(TextBuffer& buffer, int32_t value) noexcept;
// Lowercase hex digits, no prefix. Zero is "0".
bool text_append_hex(TextBuffer& buffer, uint32_t value) noexcept;
// Fixed decimals, half away from zero. Non-finite values are written as nan/inf.
bool text_append_f(TextBuffer& buffer, float value, int decimals) noexcept;
bool text_terminate(TextBuffer& buffer) noexcept;

}  // namespace mtr
