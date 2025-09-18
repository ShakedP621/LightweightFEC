#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace ltfec::util::endian {

    // ----- Write (little-endian) -----
    inline void write_u16_le(std::span<std::byte> out, std::uint16_t v) noexcept {
        if (out.size() < 2) return;
        out[0] = std::byte{ static_cast<unsigned char>(v & 0xFFu) };
        out[1] = std::byte{ static_cast<unsigned char>((v >> 8) & 0xFFu) };
    }

    inline void write_u32_le(std::span<std::byte> out, std::uint32_t v) noexcept {
        if (out.size() < 4) return;
        out[0] = std::byte{ static_cast<unsigned char>(v & 0xFFu) };
        out[1] = std::byte{ static_cast<unsigned char>((v >> 8) & 0xFFu) };
        out[2] = std::byte{ static_cast<unsigned char>((v >> 16) & 0xFFu) };
        out[3] = std::byte{ static_cast<unsigned char>((v >> 24) & 0xFFu) };
    }

    // ----- Read (little-endian) -----
    inline std::uint16_t read_u16_le(std::span<const std::byte> in) noexcept {
        if (in.size() < 2) return 0;
        const auto b0 = std::to_integer<unsigned>(in[0]);
        const auto b1 = std::to_integer<unsigned>(in[1]);
        return static_cast<std::uint16_t>(b0 | (b1 << 8));
    }

    inline std::uint32_t read_u32_le(std::span<const std::byte> in) noexcept {
        if (in.size() < 4) return 0;
        const auto b0 = std::to_integer<unsigned>(in[0]);
        const auto b1 = std::to_integer<unsigned>(in[1]);
        const auto b2 = std::to_integer<unsigned>(in[2]);
        const auto b3 = std::to_integer<unsigned>(in[3]);
        return static_cast<std::uint32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    }

} // namespace ltfec::util::endian