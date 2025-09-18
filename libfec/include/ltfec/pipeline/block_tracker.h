#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <ltfec/pipeline/policy.h>

namespace ltfec::pipeline {

    class BlockTracker {
    public:
        explicit BlockTracker(BlockPolicy p) : policy_(p), data_seen_(p.N, false) {}

        void start(std::uint64_t now_ms) {
            started_ = true;
            start_ms_ = last_ms_ = now_ms;
            std::fill(data_seen_.begin(), data_seen_.end(), false);
            parity_seen_ = false;
        }

        void mark_data(std::uint16_t seq_in_block, std::uint64_t now_ms) {
            if (!started_) start(now_ms);
            last_ms_ = now_ms;
            if (seq_in_block < policy_.N) data_seen_[seq_in_block] = true;
        }

        void mark_parity(std::uint8_t /*parity_index*/, std::uint64_t now_ms) {
            if (!started_) start(now_ms);
            last_ms_ = now_ms;
            parity_seen_ = true;
        }

        bool have_all_data() const {
            return std::all_of(data_seen_.begin(), data_seen_.end(), [](bool b) { return b; });
        }
        bool have_parity() const { return parity_seen_; }

        std::uint64_t age_ms(std::uint64_t now_ms) const {
            if (!started_) return 0;
            return (now_ms >= start_ms_) ? (now_ms - start_ms_) : 0;
        }

        bool should_close(std::uint64_t now_ms) const {
            if (!started_) return false;
            if (have_parity() && have_all_data()) return true;

            const auto span_ms = block_span_ms();
            const std::uint32_t min_ms = std::min<std::uint32_t>(60u, 2u * span_ms);
            const std::uint64_t age = age_ms(now_ms);

            if (age >= policy_.reorder_ms) return true;
            if (age >= min_ms) return true;
            return false;
        }

        void reset() {
            started_ = false;
            parity_seen_ = false;
            std::fill(data_seen_.begin(), data_seen_.end(), false);
            start_ms_ = last_ms_ = 0;
        }

        const BlockPolicy& policy() const noexcept { return policy_; }

    private:
        std::uint32_t block_span_ms() const {
            if (policy_.fps == 0) return 0;
            const std::uint64_t num = 1000ull * policy_.N + (policy_.fps - 1);
            return static_cast<std::uint32_t>(num / policy_.fps);
        }

        BlockPolicy policy_;
        bool started_{ false };
        bool parity_seen_{ false };
        std::vector<bool> data_seen_;
        std::uint64_t start_ms_{ 0 };
        std::uint64_t last_ms_{ 0 };
    };

} // namespace ltfec::pipeline