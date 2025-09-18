#include <ltfec/fec_core/block_xor.h>
#include <ltfec/fec_core/xor_parity.h>
#include <algorithm>

namespace ltfec::fec_core {

    void block_xor_parity(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<std::byte> out_parity) noexcept
    {
        xor_parity_1(data_frames, frame_len, out_parity);
    }

    int block_xor_recover_one(std::span<const std::byte* const> data_frames,
        std::size_t frame_len,
        std::span<const std::byte> parity,
        std::span<std::byte> out_recovered) noexcept
    {
        if (frame_len == 0 || out_recovered.size() < frame_len || parity.size() < frame_len) {
            return -1;
        }

        // Find the single missing index; fail if not exactly one.
        int missing = -1;
        for (std::size_t i = 0; i < data_frames.size(); ++i) {
            if (data_frames[i] == nullptr) {
                if (missing != -1) return -1; // more than one missing
                missing = static_cast<int>(i);
            }
        }
        if (missing == -1) return -1; // none missing

        // Start with parity, then XOR all present frames.
        std::copy_n(parity.begin(), frame_len, out_recovered.begin());
        for (std::size_t i = 0; i < data_frames.size(); ++i) {
            const std::byte* f = data_frames[i];
            if (!f) continue;
            for (std::size_t j = 0; j < frame_len; ++j) {
                out_recovered[j] ^= f[j];
            }
        }
        return missing;
    }

} // namespace ltfec::fec_core