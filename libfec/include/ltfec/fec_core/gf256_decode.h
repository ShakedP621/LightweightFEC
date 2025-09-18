#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

// Recover m missing data frames (m <= K <= 4) using up to K parity rows.
// Parity row j uses coefficient α^(j*d) for data index d (same scheme as encoder).
// - data_ptrs:  size N, nullptr where missing
// - parity_ptrs: size K, nullptr if parity row missing/not arrived
// - frame_len:  bytes per frame (all equal)
// - missing_indices: size m, 0..N-1 indices for missing data frames
// - out_recovered:   size m, buffers to write recovered payloads (len >= frame_len)
// Returns true iff all m frames are recovered.
namespace ltfec::fec_core {
    bool gf256_recover_erasures_vandermonde(std::span<const std::byte* const> data_ptrs,
        std::span<const std::byte* const> parity_ptrs,
        std::size_t frame_len,
        std::span<const std::uint16_t> missing_indices,
        std::span<std::byte*> out_recovered) noexcept;
} // namespace ltfec::fec_core