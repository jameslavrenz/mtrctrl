#pragma once

#include <cstdint>

// Split product into (prescaler + 1) * (autoreload + 1), both in 1..65536.
inline bool split_timer_product(uint32_t product, uint32_t& prescaler, uint32_t& autoreload) {
    if (product == 0u) {
        return false;
    }
    for (uint32_t divisor = 1; divisor <= 65536u; ++divisor) {
        if ((product % divisor) != 0u) {
            continue;
        }
        const uint32_t other = product / divisor;
        if (other >= 1u && other <= 65536u) {
            prescaler = divisor - 1u;
            autoreload = other - 1u;
            return true;
        }
    }
    return false;
}
