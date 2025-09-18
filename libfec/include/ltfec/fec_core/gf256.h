#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

// Placeholders for GF(256) parity with 2..4 parity frames.
namespace ltfec::fec_core {

    void gf256_encode(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<std::byte*> parity_frames /* K in [2..4] */) noexcept;

} // namespace ltfec::fec_core