#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>
#include <chrono>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/pipeline/fec_encoder.h>

namespace ltfec::pipeline {

    // Config for one encoder instance.
    struct TxConfig {
        std::uint16_t N{ 8 };
        std::uint16_t K{ 1 };
        // Optional limit to enforce (≈1200–1300B per DESIGN.md). 0 = no check.
        std::uint16_t max_payload_len{ 1300 };
    };

    // Assembles a full block into on-wire frames (N data + K parity).
    // Requires all data payloads to be equal length.
    class TxBlockAssembler {
    public:
        explicit TxBlockAssembler(TxConfig cfg, std::uint32_t gen_seed = default_seed())
            : cfg_(cfg), enc_(FecEncoderConfig{ cfg.N, cfg.K, 0 }), next_gen_id_(gen_seed) {
        }

        // Builds frames for one block.
        // IN:  data_payloads.size() == N, all same length.
        // OUT: out_frames resized to N+K; each entry is a full encoded datagram.
        // Returns true on success; false if validation fails.
        bool assemble_block(const std::vector<std::span<const std::byte>>& data_payloads,
            std::vector<std::vector<std::byte>>& out_frames) noexcept;

        // Expose the generation id that will be used next.
        std::uint32_t peek_next_gen() const noexcept { return next_gen_id_; }

        // Helper to compute encoded frame size.
        static std::size_t encoded_size_for(std::size_t payload_len, bool parity) {
            return ltfec::protocol::encoded_size(payload_len, parity);
        }

    private:
        static std::uint32_t default_seed() {
            using clock = std::chrono::steady_clock;
            auto v = static_cast<std::uint64_t>(clock::now().time_since_epoch().count());
            return static_cast<std::uint32_t>(v ^ (v >> 32));
        }

        TxConfig cfg_;
        FecEncoder enc_;
        std::uint32_t next_gen_id_{ 1 };
    };

} // namespace ltfec::pipeline