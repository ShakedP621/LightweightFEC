#pragma once
#include <cstdint>
#include <type_traits>

namespace ltfec::protocol {

    // Protocol version for initial wire format.
    inline constexpr std::uint8_t k_protocol_version = 1;

    // Flags2 carries parity_count_minus_one (per DESIGN.md).
    // We reserve the low 8 bits for (K-1). Upper bits reserved.
    inline constexpr std::uint16_t flags2_pack_parity_count_minus_one(std::uint16_t k) noexcept {
        return static_cast<std::uint16_t>((k - 1u) & 0x00FFu);
    }
    inline constexpr std::uint16_t flags2_get_parity_count_minus_one(std::uint16_t flags2) noexcept {
        return static_cast<std::uint16_t>(flags2 & 0x00FFu);
    }

#pragma pack(push, 1)
    struct BaseHeader {
        std::uint8_t  version;     // = k_protocol_version
        std::uint8_t  flags1;      // reserved for future use (e.g., crc enabled)
        std::uint16_t flags2;      // low 8 bits = parity_count_minus_one
        std::uint32_t fec_gen_id;  // generation/block identifier
        std::uint16_t seq_in_block;// 0..N-1 for data; parity uses subheader index
        std::uint16_t data_count;  // N
        std::uint16_t parity_count;// K
        std::uint16_t payload_len; // bytes (<= ~1300 per design, excluding headers)
    };
    static_assert(std::is_trivially_copyable_v<BaseHeader>, "BaseHeader must be POD");
    static_assert(sizeof(BaseHeader) == 16, "Header size should be stable (16B)");
#pragma pack(pop)

#pragma pack(push, 1)
    struct ParitySubheader {
        std::uint8_t  fec_scheme_id;   // see protocol::fec_scheme_id
        std::uint8_t  fec_parity_index;// 0..K-1
    };
    static_assert(sizeof(ParitySubheader) == 2, "ParitySubheader should be 2B");
#pragma pack(pop)

} // namespace ltfec::protocol