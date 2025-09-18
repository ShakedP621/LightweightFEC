#pragma once
#include <cstdint>

namespace ltfec::protocol {

    // Forward set of FEC scheme identifiers.
    // 1 = baseline XOR (K=1), 10..13 reserved for GF(256) with K=2..4 (subject to DESIGN.md).
    enum class fec_scheme_id : std::uint8_t {
        xor_k1 = 1,
        gf256_k2 = 10,
        gf256_k3 = 11,
        gf256_k4 = 12,
    };
} // namespace ltfec::protocol