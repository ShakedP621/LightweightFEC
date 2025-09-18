#include <ltfec/fec_core/xor_parity.h>
#include <algorithm>
#include <cassert>

namespace ltfec::fec_core {

    // Computes XOR parity over N data frames of equal length.
    // - data_frames: pointers to N data frame byte buffers
    // - frame_len:   number of bytes per data frame (same for all frames)
    // - out_parity:  destination buffer; only first frame_len bytes are written
    //
    // Notes:
    // * Function is noexcept and does best-effort bounds guarding (will clamp to out_parity.size()).
    // * If data_frames.size() == 0, parity is all zeros.
    void xor_parity_1(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<std::byte> out_parity) noexcept
    {
        if (frame_len == 0 || out_parity.empty()) {
            return;
        }

        // Clamp to provided parity buffer size for safety.
        const std::size_t len = std::min<std::size_t>(frame_len, out_parity.size());

        // Start with zeros.
        std::fill_n(out_parity.begin(), len, std::byte{ 0 });

        // XOR each frame into the parity buffer.
        for (const std::byte* frame : data_frames) {
            if (!frame) continue; // tolerate null pointers defensively
            for (std::size_t i = 0; i < len; ++i) {
                out_parity[i] ^= frame[i];
            }
        }
    }

} // namespace ltfec::fec_core