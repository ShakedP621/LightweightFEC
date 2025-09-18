#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <optional>
#include <span>
#include <ltfec/pipeline/policy.h>
#include <ltfec/pipeline/block_state.h>
#include <ltfec/pipeline/block_tracker.h>
#include <ltfec/protocol/frame.h>

namespace ltfec::pipeline {

    // Runtime config for RX.
    struct RxConfig {
        std::uint32_t reorder_ms{ 50 };
        std::uint32_t fps{ 30 };
        std::uint16_t max_payload_len{ 1300 };
    };

    // Snapshot for tests/metrics.
    struct RxSnapshot {
        std::uint16_t N{ 0 }, K{ 0 };
        std::uint16_t data_seen{ 0 }, parity_seen{ 0 };
        bool have_all_data{ false };
        bool have_any_parity{ false };
        std::uint16_t payload_len{ 0 };
    };

    // Internal per-generation state.
    class RxBlock {
    public:
        RxBlock(std::uint32_t gen, std::uint16_t N, std::uint16_t K, std::uint16_t payload_len, const RxConfig& cfg)
            : gen_(gen),
            policy_{ N, K, cfg.reorder_ms, cfg.fps },
            state_(policy_),
            tracker_(policy_),
            payload_len_(payload_len),
            data_(N),
            parity_(K)
        {
        }

        // Store one frame (copy payload) and update trackers.
        // Inputs are trusted to be consistent with constructor N/K/payload_len.
        void ingest(std::uint64_t now_ms,
            bool is_parity,
            std::uint16_t seq_in_block,
            std::uint8_t parity_index,
            std::span<const std::byte> payload)
        {
            if (!started_) { started_ = true; tracker_.start(now_ms); start_ms_ = now_ms; }
            if (is_parity) {
                if (parity_index < parity_.size()) {
                    parity_[parity_index].assign(payload.begin(), payload.end());
                    state_.mark_parity(parity_index);
                    tracker_.mark_parity(parity_index, now_ms);
                }
            }
            else {
                if (seq_in_block < data_.size()) {
                    data_[seq_in_block].assign(payload.begin(), payload.end());
                    state_.mark_data(seq_in_block);
                    tracker_.mark_data(seq_in_block, now_ms);
                }
            }
            last_ms_ = now_ms;
        }

        bool should_close(std::uint64_t now_ms) const { return tracker_.should_close(now_ms); }

        RxSnapshot snapshot() const {
            RxSnapshot s;
            s.N = policy_.N; s.K = policy_.K;
            s.data_seen = state_.data_seen_count();
            s.parity_seen = state_.parity_seen_count();
            s.have_all_data = state_.have_all_data();
            s.have_any_parity = state_.have_any_parity();
            s.payload_len = payload_len_;
            return s;
        }

        std::uint32_t gen() const noexcept { return gen_; }
        std::uint16_t payload_len() const noexcept { return payload_len_; }

    private:
        std::uint32_t gen_{ 0 };
        BlockPolicy policy_;
        BlockState state_;
        BlockTracker tracker_;
        std::uint16_t payload_len_{ 0 };

        bool started_{ false };
        std::uint64_t start_ms_{ 0 }, last_ms_{ 0 };

        std::vector<std::vector<std::byte>> data_;   // [N][payload_len]
        std::vector<std::vector<std::byte>> parity_; // [K][payload_len]
    };

    class RxBlockTable {
    public:
        explicit RxBlockTable(RxConfig cfg) : cfg_(cfg) {}

        // Ingest a decoded frame; creates the block if needed.
        // Returns false if rejected due to shape mismatch (payload len or N/K).
        bool ingest(std::uint64_t now_ms,
            const ltfec::protocol::BaseHeader& h,
            bool has_parity_sub,
            const ltfec::protocol::ParitySubheader& ps,
            std::span<const std::byte> payload)
        {
            if (payload.empty()) return false;
            if (cfg_.max_payload_len && payload.size() > cfg_.max_payload_len) return false;
            const auto key = h.fec_gen_id;

            auto it = blocks_.find(key);
            if (it == blocks_.end()) {
                // Create new block with shape from the header.
                RxBlock blk(key, h.data_count, h.parity_count, static_cast<std::uint16_t>(payload.size()), cfg_);
                auto [ins_it, ok] = blocks_.emplace(key, std::move(blk));
                it = ins_it;
            }
            else {
                // Validate shape consistency
                if (it->second.payload_len() != payload.size()) return false;
            }

            // Parity or data?
            if (has_parity_sub) {
                it->second.ingest(now_ms, /*is_parity*/true, /*seq*/0, ps.fec_parity_index, payload);
            }
            else {
                it->second.ingest(now_ms, /*is_parity*/false, h.seq_in_block, 0, payload);
            }
            return true;
        }

        bool should_close(std::uint32_t gen, std::uint64_t now_ms) const {
            auto it = blocks_.find(gen);
            if (it == blocks_.end()) return false;
            return it->second.should_close(now_ms);
        }

        std::optional<RxSnapshot> snapshot(std::uint32_t gen) const {
            auto it = blocks_.find(gen);
            if (it == blocks_.end()) return std::nullopt;
            return it->second.snapshot();
        }

    private:
        RxConfig cfg_;
        std::unordered_map<std::uint32_t, RxBlock> blocks_;
    };

} // namespace ltfec::pipeline