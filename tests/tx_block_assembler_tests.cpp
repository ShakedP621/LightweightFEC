#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/protocol/frame_builder.h>
#include <string>
#include <vector>
#include <cstddef>

using namespace ltfec::pipeline;
using namespace ltfec::protocol;

static std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
    return v;
}
static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

BOOST_AUTO_TEST_SUITE(tx_block_assembler_suite)

BOOST_AUTO_TEST_CASE(builds_N_plus_K_and_sets_headers) {
    TxConfig cfg{ .N = 3, .K = 1, .max_payload_len = 1300 };
    TxBlockAssembler tx(cfg, /*gen_seed*/ 100u);

    const auto d0 = to_bytes("AAAAAA");
    const auto d1 = to_bytes("BBBBBB");
    const auto d2 = to_bytes("CCCCCC");
    std::vector<std::span<const std::byte>> data = {
        std::span<const std::byte>(d0.data(), d0.size()),
        std::span<const std::byte>(d1.data(), d1.size()),
        std::span<const std::byte>(d2.data(), d2.size())
    };

    std::vector<std::vector<std::byte>> frames;
    BOOST_TEST(tx.assemble_block(data, frames));
    BOOST_TEST(frames.size() == 4u); // N + K

    // Decode and verify
    BaseHeader h{};
    bool has_parity = false;
    ParitySubheader ps{};
    std::span<const std::byte> payload;
    std::uint32_t crc = 0;

    // Data frames 0..2
    for (size_t i = 0; i < 3; ++i) {
        BOOST_TEST(decode_frame(std::span<const std::byte>(frames[i].data(), frames[i].size()),
            h, has_parity, ps, payload, crc));
        BOOST_TEST(!has_parity);
        BOOST_TEST(h.seq_in_block == i);
        BOOST_TEST(h.data_count == 3u);
        BOOST_TEST(h.parity_count == 1u);
        BOOST_TEST(h.payload_len == d0.size());
        BOOST_TEST(verify_payload_crc(payload, crc));
    }

    // Parity frame
    BOOST_TEST(decode_frame(std::span<const std::byte>(frames[3].data(), frames[3].size()),
        h, has_parity, ps, payload, crc));
    BOOST_TEST(has_parity);
    BOOST_TEST(h.seq_in_block == 3u); // N + 0
    BOOST_TEST(ps.fec_parity_index == 0u);
    BOOST_TEST(verify_payload_crc(payload, crc));

    // Check XOR relation for K=1
    for (size_t i = 0; i < payload.size(); ++i) {
        const auto want = d0[i] ^ d1[i] ^ d2[i];
        BOOST_TEST(u8(payload[i]) == u8(want));
    }
}

BOOST_AUTO_TEST_CASE(gen_id_increments_per_block) {
    TxConfig cfg{ .N = 2, .K = 1, .max_payload_len = 1300 };
    TxBlockAssembler tx(cfg, /*gen_seed*/ 555u);

    const auto a0 = to_bytes("pppp");
    const auto a1 = to_bytes("qqqq");
    const auto b0 = to_bytes("rrrr");
    const auto b1 = to_bytes("ssss");

    std::vector<std::vector<std::byte>> f1, f2;
    std::vector<std::span<const std::byte>> A = {
        {a0.data(), a0.size()}, {a1.data(), a1.size()}
    };
    std::vector<std::span<const std::byte>> B = {
        {b0.data(), b0.size()}, {b1.data(), b1.size()}
    };

    BOOST_TEST(tx.assemble_block(A, f1));
    BOOST_TEST(tx.assemble_block(B, f2));

    BaseHeader h1{}, h2{};
    bool p; ParitySubheader ps{}; std::span<const std::byte> pay; std::uint32_t c;

    BOOST_TEST(decode_frame(std::span<const std::byte>(f1[0].data(), f1[0].size()), h1, p, ps, pay, c));
    BOOST_TEST(decode_frame(std::span<const std::byte>(f2[0].data(), f2[0].size()), h2, p, ps, pay, c));
    BOOST_TEST(h2.fec_gen_id == h1.fec_gen_id + 1u);
}

BOOST_AUTO_TEST_SUITE_END()