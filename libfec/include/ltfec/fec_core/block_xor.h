#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace ltfec::fec_core {

    // Compute XOR parity for N data frames (equal length).
    // - data_frames: span of pointers to each data frame (must all be non-null here)
    // - frame_len:   bytes per frame (same for all frames)
    // - out_parity:  buffer to receive parity; only first frame_len bytes are written.
    void block_xor_parity(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<std::byte> out_parity) noexcept;

    // Attempt to recover ONE missing frame given XOR parity and the other frames.
    // - data_frames: pointers to N frames; the missing one must be nullptr
    // - frame_len:   bytes per frame
    // - parity:      the parity frame (length >= frame_len)
    // - out_recovered: output buffer for the recovered frame (length >= frame_len)
    // Returns the index [0..N-1] of the recovered frame on success, or -1 on failure.
    int block_xor_recover_one(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<const std::byte> parity,
        std::span<std::byte> out_recovered) noexcept;

} // namespace ltfec::fec_core