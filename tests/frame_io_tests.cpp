#include <boost/test/unit_test.hpp>   // not the included runner here
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_io.h>
#include <array>
#include <string>

using namespace ltfec::protocol;

static std::span<const std::byte> to_span(std::string const& s) {
    return std::span<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
}

BOOST_AUTO_TEST_SUITE(frame_io_suite)

BOOST_AUTO_TEST_CASE(flags2_pack_unpack) {
    std::uint16_t f = flags2_pack_parity_count_minus_one(1);
    BOOST_TEST(flags2_get_parity_count_minus_one(f) == 0u);

    f = flags2_pack_parity_count_minus_one(4);
    BOOST_TEST(flags2_get_parity_count_minus_one(f) == 3u);
}

BOOST_AUTO_TEST_CASE(base_header_roundtrip) {
    BaseHeader h{};
    h.version = k_protocol_version;
    h.flags1 = 0xA5;
    h.flags2 = flags2_pack_parity_count_minus_one(1);
    h.fec_gen_id = 0x11223344u;
    h.seq_in_block = 7;
    h.data_count = 8;
    h.parity_count = 1;
    h.payload_len = 1200;

    std::array<std::byte, sizeof(BaseHeader)> buf{};
    BOOST_TEST(write_base_header(std::span<std::byte>(buf.data(), buf.size()), h));

    BaseHeader r{};
    BOOST_TEST(read_base_header(std::span<const std::byte>(buf.data(), buf.size()), r));

    BOOST_TEST(r.version == h.version);
    BOOST_TEST(r.flags1 == h.flags1);
    BOOST_TEST(r.flags2 == h.flags2);
    BOOST_TEST(r.fec_gen_id == h.fec_gen_id);
    BOOST_TEST(r.seq_in_block == h.seq_in_block);
    BOOST_TEST(r.data_count == h.data_count);
    BOOST_TEST(r.parity_count == h.parity_count);
    BOOST_TEST(r.payload_len == h.payload_len);
}

BOOST_AUTO_TEST_CASE(crc32c_payload_known_vector) {
    const std::string s = "123456789";
    // Standard CRC32C for "123456789"
    BOOST_TEST(crc32c_payload(to_span(s)) == 0xE3069283u);
}

BOOST_AUTO_TEST_SUITE_END()