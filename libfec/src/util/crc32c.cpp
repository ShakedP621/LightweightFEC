#include <ltfec/util/crc32c.h>
#include <array>

namespace ltfec::util {

    // Reflected polynomial for CRC32C (Castagnoli)
    static constexpr uint32_t kPoly = 0x82F63B78u;

    // Lazily build the 256-entry lookup table once (thread-safe in C++11+)
    static const std::array<uint32_t, 256>& table() {
        static std::array<uint32_t, 256> t{};
        static bool inited = false;
        if (!inited) {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1u) ? (c >> 1) ^ kPoly : (c >> 1);
                }
                t[i] = c;
            }
            inited = true;
        }
        return t;
    }

    static inline uint8_t b2u(std::byte b) noexcept {
        return std::to_integer<uint8_t>(b);
    }

    uint32_t crc32c_update(uint32_t state, std::span<const std::byte> data) noexcept {
        const auto& T = table();
        uint32_t crc = state;
        for (std::byte b : data) {
            crc = T[(crc ^ b2u(b)) & 0xFFu] ^ (crc >> 8);
        }
        return crc;
    }

    uint32_t crc32c(std::span<const std::byte> data) noexcept {
        return crc32c_finish(crc32c_update(crc32c_init(), data));
    }

    uint32_t crc32c(std::string_view s) noexcept {
        const auto* ptr = reinterpret_cast<const std::byte*>(s.data());
        return crc32c(std::span<const std::byte>(ptr, s.size()));
    }

    uint32_t crc32c(const void* data, std::size_t len) noexcept {
        const auto* ptr = reinterpret_cast<const std::byte*>(data);
        return crc32c(std::span<const std::byte>(ptr, len));
    }

} // namespace ltfec::util