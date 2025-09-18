#pragma once
#include <cstdint>

namespace ltfec::sim {

    // Bernoulli + Gilbert–Elliott (placeholders; full impl later)
    struct Bernoulli {
        double p_loss{ 0.0 }; // in [0,1]
    };

    struct GilbertElliott {
        double p_g_to_b{ 0.0 }; // Good->Bad transition
        double p_b_to_g{ 0.0 }; // Bad->Good transition
        double p_loss_bad{ 0.0 };
    };

    struct JitterUniform {
        std::uint32_t J_ms{ 0 }; // jitter range [0,J] ms
    };

} // namespace ltfec::sim