#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <optional>
#include <span>
#include <algorithm>
#include <ltfec/fec_core/gf256_decode.h>
#include <ltfec/pipeline/policy.h>
#include <ltfec/pipeline/block_state.h>
#include <ltfec/pipeline/block_tracker.h>
#include <ltfec/fec_core/block_xor.h>
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

    // Finalized/closed block result (data payloads in arrival order 0..N-1).
    struct RxClosedBlock {
        std::uint32_t gen{ 0 };
        std::uint16_t N{ 0 }, K{ 0 };
        std::uint16_t payload_len{ 0 };
        std::vector<std::vector<std::byte>> data;   // size N, each payload_len bytes
        std::vector<bool> was_recovered;            // size N, true if payload was reconstructed
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

        // Build the closed-block result (perform XOR single-erasure recovery for K=1 if possible).
        RxClosedBlock extract_closed() const {
            RxClosedBlock out;
            out.gen = gen_;
            out.N = policy_.N;
            out.K = policy_.K;
            out.payload_len = payload_len_;
            out.data.resize(policy_.N);
            out.was_recovered.assign(policy_.N, false);

            // Copy present data payloads
            for (std::uint16_t i = 0; i < policy_.N; ++i) {
                out.data[i] = data_[i]; // may be empty if missing
            }

            // --- K=1 XOR recovery (existing path) ---
            if (policy_.K == 1 && state_.recoverable_k1() && parity_.size() >= 1 &&
                parity_[0].size() == payload_len_)
            {
                std::vector<const std::byte*> ptrs(policy_.N, nullptr);
                int missing = -1;
                for (std::uint16_t i = 0; i < policy_.N; ++i) {
                    if (data_[i].size() == payload_len_) ptrs[i] = data_[i].data();
                    else {
                        if (missing != -1) { missing = -1; break; }
                        missing = static_cast<int>(i);
                    }
                }
                if (missing >= 0) {
                    std::vector<std::byte> rec(payload_len_);
                    const int idx = ltfec::fec_core::block_xor_recover_one(
                        std::span<const std::byte* const>(ptrs.data(), ptrs.size()),
                        payload_len_,
                        std::span<const std::byte>(parity_[0].data(), parity_[0].size()),
                        std::span<std::byte>(rec.data(), rec.size()));
                    if (idx == missing) {
                        out.data[static_cast<std::size_t>(idx)] = std::move(rec);
                        out.was_recovered[static_cast<std::size_t>(idx)] = true;
                    }
                }
                return out;
            }

            // --- K >= 2 GF(256) recovery: recover up to K missing if enough parity rows arrived ---
            if (policy_.K >= 2) {
                // Gather missing indices
                std::vector<std::uint16_t> miss;
                for (std::uint16_t i = 0; i < policy_.N; ++i) {
                    if (data_[i].size() != payload_len_) miss.push_back(i);
                }
                if (!miss.empty() && miss.size() <= policy_.K) {
                    // Prepare pointers
                    std::vector<const std::byte*> data_ptrs(policy_.N, nullptr);
                    for (std::uint16_t i = 0; i < policy_.N; ++i) {
                        if (data_[i].size() == payload_len_) data_ptrs[i] = data_[i].data();
                    }
                    std::vector<const std::byte*> parity_ptrs(policy_.K, nullptr);
                    for (std::uint16_t j = 0; j < policy_.K; ++j) {
                        if (parity_[j].size() == payload_len_) parity_ptrs[j] = parity_[j].data();
                    }
                    std::vector<std::vector<std::byte>> recs(miss.size(), std::vector<std::byte>(payload_len_));
                    std::vector<std::byte*> out_ptrs(miss.size());
                    for (std::size_t i = 0; i < miss.size(); ++i) out_ptrs[i] = recs[i].data();

                    if (ltfec::fec_core::gf256_recover_erasures_vandermonde(
                        std::span<const std::byte* const>(data_ptrs.data(), data_ptrs.size()),
                        std::span<const std::byte* const>(parity_ptrs.data(), parity_ptrs.size()),
                        payload_len_,
                        std::span<const std::uint16_t>(miss.data(), miss.size()),
                        std::span<std::byte*>(out_ptrs.data(), out_ptrs.size())))
                    {
                        // Success: move recovered payloads into outputs
                        for (std::size_t k = 0; k < miss.size(); ++k) {
                            const auto idx = static_cast<std::size_t>(miss[k]);
                            out.data[idx] = std::move(recs[k]);
                            out.was_recovered[idx] = true;
                        }
                    }
                }
            }

            return out;
        }


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
                RxBlock blk(key, h.data_count, h.parity_count, static_cast<std::uint16_t>(payload.size()), cfg_);
                auto [ins_it, ok] = blocks_.emplace(key, std::move(blk));
                it = ins_it;
            }
            else {
                if (it->second.payload_len() != payload.size()) return false;
            }

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

        // If the block is ready to close, extract it (with recovery where possible) and erase from table.
        bool close_if_ready(std::uint32_t gen, std::uint64_t now_ms, RxClosedBlock& out) {
            auto it = blocks_.find(gen);
            if (it == blocks_.end()) return false;
            if (!it->second.should_close(now_ms)) return false;
            out = it->second.extract_closed();
            blocks_.erase(it);
            return true;
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