#pragma once
#include <cstdint>
#include <algorithm>
#include <ltfec/sim/rng.h>

namespace ltfec::sim {

    // Bernoulli loss: drop with probability p_loss each trial
    struct BernoulliLoss {
        double p_loss{ 0.0 }; // in [0,1]
        bool drop(XorShift32& rng) noexcept {
            const double p = std::clamp(p_loss, 0.0, 1.0);
            if (p <= 0.0) return false;
            if (p >= 1.0) return true;
            return rng.next_unit() < p;
        }
    };

    // Gilbert–Elliott: two-state Markov (Good/Bad) with per-bad-state drop prob
    struct GilbertElliottLoss {
        double p_g_to_b{ 0.0 };
        double p_b_to_g{ 0.0 };
        double p_loss_bad{ 0.0 };
        bool bad{ false }; // current state: false=Good, true=Bad

        bool drop(XorShift32& rng) noexcept {
            const double pg = std::clamp(p_g_to_b, 0.0, 1.0);
            const double pb = std::clamp(p_b_to_g, 0.0, 1.0);
            const double pl = std::clamp(p_loss_bad, 0.0, 1.0);

            // Transition first
            const double u = rng.next_unit();
            if (!bad) {
                if (u < pg) bad = true;
            }
            else {
                if (u < pb) bad = false;
            }

            if (!bad) return false;
            if (pl <= 0.0) return false;
            if (pl >= 1.0) return true;
            return rng.next_unit() < pl;
        }
    };

    // Jitter uniform in [0, J] milliseconds
    inline std::uint32_t jitter_uniform_ms(XorShift32& rng, std::uint32_t J_ms) noexcept {
        if (J_ms == 0) return 0;
        return static_cast<std::uint32_t>(rng.next_u32() % (J_ms + 1u));
    }

} // namespace ltfec::sim