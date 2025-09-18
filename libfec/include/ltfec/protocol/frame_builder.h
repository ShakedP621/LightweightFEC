#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_io.h>
#include <ltfec/util/endian.h>
#include <ltfec/util/crc32c.h>

namespace ltfec::protocol {

    struct FrameSizes {
        static constexpr std::size_t kBase = sizeof(BaseHeader);          // 16
        static constexpr std::size_t kParitySub = sizeof(ParitySubheader);// 2
        static constexpr std::size_t kCrcTrailer = 4;                      // u32 LE
    };

    // Returns total encoded size for a given payload_len and whether parity subheader is present.
    inline std::size_t encoded_size(std::size_t payload_len, bool with_parity_subheader) noexcept {
        return FrameSizes::kBase
            + (with_parity_subheader ? FrameSizes::kParitySub : 0)
            + payload_len
            + FrameSizes::kCrcTrailer;
    }

    // Detect parity-frame from header values, per DESIGN.md:
    // data frames: seq_in_block ∈ [0..N-1], parity frames: seq indexes parity and carry subheader.
    inline bool is_parity_frame(const BaseHeader& h) noexcept {
        return h.seq_in_block >= h.data_count;
    }

    // Write:
    //   BaseHeader
    //   (optional ParitySubheader for parity frames)
    //   payload bytes
    //   CRC32C(payload) as u32 LE
    // Returns false if out buffer too small or header/payload length mismatch.
    inline bool encode_data_frame(std::span<std::byte> out,
        const BaseHeader& h,
        std::span<const std::byte> payload) noexcept
    {
        if (is_parity_frame(h)) return false; // use encode_parity_frame for parity
        if (payload.size() != h.payload_len) return false;

        const std::size_t need = encoded_size(payload.size(), /*with_parity_subheader=*/false);
        if (out.size() < need) return false;

        // Write header
        if (!write_base_header(out.first(FrameSizes::kBase), h)) return false;

        // Payload
        std::memcpy(out.data() + FrameSizes::kBase, payload.data(), payload.size());

        // CRC trailer (payload-only)
        const std::uint32_t crc = ltfec::util::crc32c(payload);
        ltfec::util::endian::write_u32_le(out.subspan(FrameSizes::kBase + payload.size(), 4), crc);
        return true;
    }

    inline bool encode_parity_frame(std::span<std::byte> out,
        const BaseHeader& h,
        const ParitySubheader& ps,
        std::span<const std::byte> payload) noexcept
    {
        if (!is_parity_frame(h)) return false; // seq_in_block should denote parity index
        if (payload.size() != h.payload_len) return false;

        const std::size_t need = encoded_size(payload.size(), /*with_parity_subheader=*/true);
        if (out.size() < need) return false;

        // header
        if (!write_base_header(out.first(FrameSizes::kBase), h)) return false;

        // parity subheader (2B)
        auto* p = out.data() + FrameSizes::kBase;
        p[0] = std::byte{ ps.fec_scheme_id };
        p[1] = std::byte{ ps.fec_parity_index };

        // payload
        std::memcpy(p + FrameSizes::kParitySub, payload.data(), payload.size());

        // trailer
        const std::uint32_t crc = ltfec::util::crc32c(payload);
        ltfec::util::endian::write_u32_le(std::span<std::byte>(p + FrameSizes::kParitySub + payload.size(), 4), crc);
        return true;
    }

    // Parse an encoded frame buffer and expose views.
    // On success, returns true and sets:
    //   h_out          = parsed BaseHeader
    //   has_parity_sub = true if parity subheader was present (deduced via is_parity_frame())
    //   ps_out         = filled only if has_parity_sub == true
    //   payload_out    = span view of payload bytes (within 'in')
    //   crc_out        = parsed CRC32C trailer value
    // Performs only size checks; no CRC verification here.
    inline bool decode_frame(std::span<const std::byte> in,
        BaseHeader& h_out,
        bool& has_parity_sub,
        ParitySubheader& ps_out,
        std::span<const std::byte>& payload_out,
        std::uint32_t& crc_out) noexcept
    {
        // Need at least base header + trailer
        if (in.size() < FrameSizes::kBase + FrameSizes::kCrcTrailer) return false;

        if (!read_base_header(in.first(FrameSizes::kBase), h_out)) return false;

        has_parity_sub = is_parity_frame(h_out);

        const std::size_t header_bytes = FrameSizes::kBase + (has_parity_sub ? FrameSizes::kParitySub : 0);
        const std::size_t total_needed = header_bytes + h_out.payload_len + FrameSizes::kCrcTrailer;
        if (in.size() < total_needed) return false;

        std::size_t off = FrameSizes::kBase;

        if (has_parity_sub) {
            ps_out.fec_scheme_id = static_cast<std::uint8_t>(std::to_integer<unsigned char>(in[off + 0]));
            ps_out.fec_parity_index = static_cast<std::uint8_t>(std::to_integer<unsigned char>(in[off + 1]));
            off += FrameSizes::kParitySub;
        }

        payload_out = in.subspan(off, h_out.payload_len);
        off += h_out.payload_len;

        crc_out = ltfec::util::endian::read_u32_le(in.subspan(off, 4));
        return true;
    }

    // Verify payload CRC32C against trailer.
    inline bool verify_payload_crc(std::span<const std::byte> payload, std::uint32_t crc) noexcept {
        return ltfec::util::crc32c(payload) == crc;
    }

} // namespace ltfec::protocol