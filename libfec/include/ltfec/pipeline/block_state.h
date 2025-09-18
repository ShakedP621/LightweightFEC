#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <ltfec/pipeline/policy.h>

namespace ltfec::pipeline {

    class BlockState {
    public:
        explicit BlockState(BlockPolicy p) : policy_(p), data_seen_(p.N, false), parity_seen_(p.K, false) {}

        void reset(BlockPolicy p) {
            policy_ = p;
            data_seen_.assign(p.N, false);
            parity_seen_.assign(p.K, false);
        }

        void mark_data(std::uint16_t seq_in_block) {
            if (seq_in_block < policy_.N) data_seen_[seq_in_block] = true;
        }
        void mark_parity(std::uint8_t parity_index) {
            if (parity_index < policy_.K) parity_seen_[parity_index] = true;
        }

        std::uint16_t data_seen_count() const {
            return static_cast<std::uint16_t>(std::count(data_seen_.begin(), data_seen_.end(), true));
        }
        std::uint16_t data_missing_count() const { return policy_.N - data_seen_count(); }

        std::uint16_t parity_seen_count() const {
            return static_cast<std::uint16_t>(std::count(parity_seen_.begin(), parity_seen_.end(), true));
        }

        bool have_all_data() const { return data_missing_count() == 0; }
        bool have_any_parity() const { return parity_seen_count() > 0; }
        bool have_all_parity() const { return parity_seen_count() == policy_.K; }

        bool recoverable_k1() const {
            return policy_.K == 1 && data_missing_count() == 1 && have_any_parity();
        }

        int first_missing_data() const {
            int idx = -1;
            for (std::uint16_t i = 0; i < policy_.N; ++i) {
                if (!data_seen_[i]) {
                    if (idx != -1) return -1;
                    idx = i;
                }
            }
            return idx;
        }

        const BlockPolicy& policy() const noexcept { return policy_; }

    private:
        BlockPolicy policy_;
        std::vector<bool> data_seen_;
        std::vector<bool> parity_seen_;
    };

} // namespace ltfec::pipeline