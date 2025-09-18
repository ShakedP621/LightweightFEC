#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

// Baseline XOR parity for K=1.
namespace ltfec::fec_core {

    void xor_parity_1(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<std::byte> out_parity) noexcept;

} // namespace ltfec::fec_core