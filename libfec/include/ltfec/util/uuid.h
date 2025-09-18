#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <ltfec/sim/rng.h>

namespace ltfec::util {

    // Simple UUID v4 generator seeded from steady_clock + address entropy.
    // Not cryptographic; good enough for run IDs.
    inline std::string uuid_v4() {
        using clock = std::chrono::steady_clock;
        auto now = static_cast<std::uint64_t>(clock::now().time_since_epoch().count());

        // Mix in some address entropy
        const void* self = static_cast<const void*>(&now);
        now ^= reinterpret_cast<std::uintptr_t>(self) * 0x9E3779B97F4A7C15ull;

        ltfec::sim::XorShift32 rng(static_cast<std::uint32_t>(now ^ (now >> 32)));

        std::array<std::uint8_t, 16> b{};
        for (auto& v : b) v = static_cast<std::uint8_t>(rng.next_u32() & 0xFF);

        // Set version (4) and variant (10xx)
        b[6] = static_cast<std::uint8_t>((b[6] & 0x0F) | 0x40);
        b[8] = static_cast<std::uint8_t>((b[8] & 0x3F) | 0x80);

        std::ostringstream os;
        os << std::hex << std::setfill('0')
            << std::setw(2) << (int)b[0] << std::setw(2) << (int)b[1] << std::setw(2) << (int)b[2] << std::setw(2) << (int)b[3] << '-'
            << std::setw(2) << (int)b[4] << std::setw(2) << (int)b[5] << '-'
            << std::setw(2) << (int)b[6] << std::setw(2) << (int)b[7] << '-'
            << std::setw(2) << (int)b[8] << std::setw(2) << (int)b[9] << '-'
            << std::setw(2) << (int)b[10] << std::setw(2) << (int)b[11] << std::setw(2) << (int)b[12] << std::setw(2) << (int)b[13] << std::setw(2) << (int)b[14] << std::setw(2) << (int)b[15];
        return os.str();
    }

} // namespace ltfec::util