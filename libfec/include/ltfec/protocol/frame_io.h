#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <ltfec/protocol/frame.h>
#include <ltfec/util/endian.h>
#include <ltfec/util/crc32c.h>

namespace ltfec::protocol {

    // Byte layout helpers for BaseHeader and CRC of payload (CRC32C over payload ONLY).

    struct FrameLayout {
        static constexpr std::size_t kBaseHeaderSize = sizeof(BaseHeader);      // 16
        static constexpr std::size_t kParitySubheaderSize = sizeof(ParitySubheader); // 2
    };

    // Serialize BaseHeader into OUT (little-endian for multi-byte fields).
    // Returns false if OUT is too small (<16), true on success.
    inline bool write_base_header(std::span<std::byte> out, const BaseHeader& h) noexcept {
        using namespace ltfec::util::endian;
        if (out.size() < sizeof(BaseHeader)) return false;
        out[0] = std::byte{ h.version };
        out[1] = std::byte{ h.flags1 };
        write_u16_le(out.subspan(2, 2), h.flags2);
        write_u32_le(out.subspan(4, 4), h.fec_gen_id);
        write_u16_le(out.subspan(8, 2), h.seq_in_block);
        write_u16_le(out.subspan(10, 2), h.data_count);
        write_u16_le(out.subspan(12, 2), h.parity_count);
        write_u16_le(out.subspan(14, 2), h.payload_len);
        return true;
    }

    // Parse BaseHeader from IN (little-endian). Returns false if IN too small.
    inline bool read_base_header(std::span<const std::byte> in, BaseHeader& h) noexcept {
        using namespace ltfec::util::endian;
        if (in.size() < sizeof(BaseHeader)) return false;
        h.version = static_cast<std::uint8_t>(std::to_integer<unsigned char>(in[0]));
        h.flags1 = static_cast<std::uint8_t>(std::to_integer<unsigned char>(in[1]));
        h.flags2 = read_u16_le(in.subspan(2, 2));
        h.fec_gen_id = read_u32_le(in.subspan(4, 4));
        h.seq_in_block = read_u16_le(in.subspan(8, 2));
        h.data_count = read_u16_le(in.subspan(10, 2));
        h.parity_count = read_u16_le(in.subspan(12, 2));
        h.payload_len = read_u16_le(in.subspan(14, 2));
        return true;
    }

    // CRC32C of payload only (for on-wire validation).
    inline std::uint32_t crc32c_payload(std::span<const std::byte> payload) noexcept {
        return ltfec::util::crc32c(payload);
    }

} // namespace ltfec::protocol