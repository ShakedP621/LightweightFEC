#pragma once
#include <cstdint>

namespace ltfec::pipeline {

    struct BlockPolicy {
        std::uint16_t N{ 8 };
        std::uint16_t K{ 1 };
        std::uint32_t reorder_ms{ 50 }; // default from DESIGN.md
        std::uint32_t fps{ 30 };        // frames per second
    };

} // namespace ltfec::pipeline