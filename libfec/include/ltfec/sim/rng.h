#pragma once
#include <cstdint>

namespace ltfec::sim {

    // Simple deterministic xorshift32 RNG (portable across platforms)
    struct XorShift32 {
        std::uint32_t state;
        explicit XorShift32(std::uint32_t seed) : state(seed ? seed : 0xA3C59AC3u) {}

        std::uint32_t next_u32() noexcept {
            std::uint32_t x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            return x;
        }

        // Uniform in [0,1)
        double next_unit() noexcept {
            // Use top 24 bits -> 1/2^24 resolution
            return (next_u32() >> 8) * (1.0 / 16777216.0);
        }
    };

} // namespace ltfec::sim