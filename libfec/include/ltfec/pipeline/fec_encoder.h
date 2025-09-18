#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <ltfec/protocol/ids.h>         // fec_scheme_id
#include <ltfec/fec_core/xor_parity.h>  // K=1
#include <ltfec/fec_core/gf256.h>       // K=2..4

namespace ltfec::pipeline {

    struct FecEncoderConfig {
        std::uint16_t N{ 8 };
        std::uint16_t K{ 1 };
        // If zero, scheme is chosen automatically from K:
        //   K==1 → xor_k1; K in [2..4] → gf256_k{K}
        std::uint8_t fec_scheme_id{ 0 };
    };

    // Thin façade that dispatches to XOR or GF(256) parity encoders.
    // All frames must be equal length.
    class FecEncoder {
    public:
        explicit FecEncoder(FecEncoderConfig cfg) : cfg_(cfg) {}

        // Encode K parity frames for the given block.
        // - data_frames: pointers to N data frames (non-null for encode)
        // - frame_len:   number of bytes per frame
        // - parity_frames: array of K output pointers (non-null)
        // Returns fec_scheme_id actually used (matching protocol::fec_scheme_id).
        std::uint8_t encode(std::span<const std::byte* const> data_frames,
            std::size_t frame_len,
            std::span<std::byte*> parity_frames) const noexcept
        {
            const auto scheme = pick_scheme_id();
            if (scheme == static_cast<std::uint8_t>(protocol::fec_scheme_id::xor_k1)) {
                // Expect exactly one parity buffer
                if (parity_frames.size() >= 1 && parity_frames[0]) {
                    ltfec::fec_core::xor_parity_1(data_frames, frame_len,
                        std::span<std::byte>(parity_frames[0], frame_len));
                }
            }
            else {
                // GF(256) supports K in [2..4]
                ltfec::fec_core::gf256_encode(data_frames, frame_len, parity_frames);
            }
            return scheme;
        }

        // Scheme chosen from cfg (see comment above)
        std::uint8_t pick_scheme_id() const noexcept {
            if (cfg_.fec_scheme_id) return cfg_.fec_scheme_id;
            if (cfg_.K == 1) return static_cast<std::uint8_t>(protocol::fec_scheme_id::xor_k1);
            if (cfg_.K == 2) return static_cast<std::uint8_t>(protocol::fec_scheme_id::gf256_k2);
            if (cfg_.K == 3) return static_cast<std::uint8_t>(protocol::fec_scheme_id::gf256_k3);
            if (cfg_.K == 4) return static_cast<std::uint8_t>(protocol::fec_scheme_id::gf256_k4);
            // Out-of-scope K: default to xor_k1 as a safe sentinel (no-op beyond first)
            return static_cast<std::uint8_t>(protocol::fec_scheme_id::xor_k1);
        }

        const FecEncoderConfig& config() const noexcept { return cfg_; }

    private:
        FecEncoderConfig cfg_;
    };

} // namespace ltfec::pipeline