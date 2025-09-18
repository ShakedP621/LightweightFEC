#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/protocol/ids.h>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>
#include <string>
#include <vector>
#include <cstddef>

using namespace ltfec::protocol;

static std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
    return v;
}
static std::span<const std::byte> to_span(const std::string& s) {
    return std::span<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
}
static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

BOOST_AUTO_TEST_SUITE(frame_builder_suite)

BOOST_AUTO_TEST_CASE(encode_decode_data_frame) {
    const std::string payload_s = "HELLO";
    const auto payload = to_bytes(payload_s);

    BaseHeader h{};
    h.version = k_protocol_version;
    h.flags1 = 0;
    h.flags2 = flags2_pack_parity_count_minus_one(1); // K=1 baseline
    h.fec_gen_id = 0xAABBCCDDu;
    h.seq_in_block = 3;     // data frame index (0..N-1)
    h.data_count = 8;
    h.parity_count = 1;
    h.payload_len = static_cast<std::uint16_t>(payload.size());

    const std::size_t need = encoded_size(payload.size(), /*with_parity_subheader=*/false);
    std::vector<std::byte> buf(need);

    BOOST_TEST(encode_data_frame(std::span<std::byte>(buf), h, std::span<const std::byte>(payload)));

    BaseHeader r{};
    bool has_parity = false;
    ParitySubheader ps{};
    std::span<const std::byte> pay_view;
    std::uint32_t crc = 0;

    BOOST_TEST(decode_frame(std::span<const std::byte>(buf), r, has_parity, ps, pay_view, crc));
    BOOST_TEST(!has_parity);
    BOOST_TEST(r.seq_in_block == h.seq_in_block);
    BOOST_TEST(r.data_count == 8);
    BOOST_TEST(r.parity_count == 1);
    BOOST_TEST(r.payload_len == payload.size());
    BOOST_TEST(pay_view.size() == payload.size());
    BOOST_TEST(verify_payload_crc(pay_view, crc));
}

BOOST_AUTO_TEST_CASE(encode_decode_parity_frame) {
    const std::string payload_s = "PARITY!";
    const auto payload = to_bytes(payload_s);

    BaseHeader h{};
    h.version = k_protocol_version;
    h.flags1 = 0;
    h.flags2 = flags2_pack_parity_count_minus_one(1);
    h.fec_gen_id = 0x01020304u;
    h.seq_in_block = 8;     // >= N => parity frame
    h.data_count = 8;
    h.parity_count = 1;
    h.payload_len = static_cast<std::uint16_t>(payload.size());

    ParitySubheader ps{};
    ps.fec_scheme_id = static_cast<std::uint8_t>(ltfec::protocol::fec_scheme_id::xor_k1);
    ps.fec_parity_index = 0;

    const std::size_t need = encoded_size(payload.size(), /*with_parity_subheader=*/true);
    std::vector<std::byte> buf(need);

    BOOST_TEST(encode_parity_frame(std::span<std::byte>(buf), h, ps, std::span<const std::byte>(payload)));

    BaseHeader r{};
    bool has_parity = false;
    ParitySubheader rps{};
    std::span<const std::byte> pay_view;
    std::uint32_t crc = 0;

    BOOST_TEST(decode_frame(std::span<const std::byte>(buf), r, has_parity, rps, pay_view, crc));
    BOOST_TEST(has_parity);
    BOOST_TEST(r.seq_in_block == h.seq_in_block);
    BOOST_TEST(rps.fec_scheme_id == ps.fec_scheme_id);
    BOOST_TEST(rps.fec_parity_index == ps.fec_parity_index);
    BOOST_TEST(pay_view.size() == payload.size());
    BOOST_TEST(verify_payload_crc(pay_view, crc));
}

BOOST_AUTO_TEST_SUITE_END()