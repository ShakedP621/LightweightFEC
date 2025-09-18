#pragma once
#include <cstdint>

namespace ltfec::pipeline {

    // Placeholder for block timing policy. We'll wire real logic later.
    struct BlockPolicy {
        std::uint16_t N{ 8 };
        std::uint16_t K{ 1 };
        std::uint32_t reorder_ms{ 50 };
    };

} // namespace ltfec::pipeline