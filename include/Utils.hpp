//
// Created by etders on 27.04.2026.
//

#ifndef FASTBIN_UTILS_HPP
#define FASTBIN_UTILS_HPP

#include <cstdint>

namespace FastBin {
    constexpr inline uint32_t fnv1_a(const char* str) {
        uint32_t hash = 0x811c9dc5; // FNV_BASIS
        while (*str) {
            hash ^= static_cast<uint32_t>(*str++);
            hash *= 0x01000193; // FNV_PRIME
        }
        return hash;
    }
}

#endif