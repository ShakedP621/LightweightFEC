#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/pipeline/rx_block_table.h>
#include <ltfec/protocol/frame_builder.h>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

using namespace ltfec::pipeline;
using namespace ltfec::protocol;

static std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
    return v;
}
static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

BOOST_AUTO_TEST_SUITE(rx_recover_gf256_suite)

BOOST_AUTO_TEST_CASE(k2_recover_two_missing) {
    // N=5, K=2 — drop two data frames and recover using both parity rows.
    TxConfig txcfg{ .N = 5, .K = 2, .max_payload_len = 1300 };
    TxBlockAssembler tx(txcfg, /*gen_seed*/ 123u);

    const auto d0 = to_bytes("aaaaaa");
    const auto d1 = to_bytes("bbbbbb");
    const auto d2 = to_bytes("cccccc");
    const auto d3 = to_bytes("dddddd");
    const auto d4 = to_bytes("eeeeee");

    std::vector<std::span<const std::byte>> data = {
        {d0.data(), d0.size()}, {d1.data(), d1.size()}, {d2.data(), d2.size()},
        {d3.data(), d3.size()}, {d4.data(), d4.size()}
    };

    std::vector<std::vector<std::byte>> frames;
    BOOST_TEST(tx.assemble_block(data, frames));
    const std::uint32_t gen = tx.peek_next_gen() - 1;

    // Decode frames upfront
    std::vector<BaseHeader> H(frames.size());
    std::vector<uint8_t> HasP(frames.size(), 0);                 // <-- byte flags (not vector<bool>)
    std::vector<ParitySubheader> PS(frames.size());
    std::vector<std::span<const std::byte>> PL(frames.size());
    std::uint32_t crc;
    for (size_t i = 0; i < frames.size(); ++i) {
        bool hasp = false;                                        // real bool for decode_frame
        BOOST_TEST(decode_frame(std::span<const std::byte>(frames[i].data(), frames[i].size()),
            H[i], hasp, PS[i], PL[i], crc));
        HasP[i] = hasp ? 1u : 0u;
    }

    // RX table
    RxBlockTable rxt({ .reorder_ms = 50, .fps = 30, .max_payload_len = 1300 });
    const std::uint64_t t0 = 1000;

    // Ingest all data except indices 1 and 3
    for (size_t i = 0; i < 5; ++i) {
        if (i == 1 || i == 3) continue;
        BOOST_TEST(rxt.ingest(t0, H[i], static_cast<bool>(HasP[i]), PS[i], PL[i]));
    }
    // Ingest both parity frames (indices 5 and 6 in frames vector)
    for (size_t i = 5; i < frames.size(); ++i) {
        BOOST_TEST(rxt.ingest(t0 + 1, H[i], static_cast<bool>(HasP[i]), PS[i], PL[i]));
    }

    // Close by time
    BOOST_TEST(!rxt.should_close(gen, t0 + 49));
    BOOST_TEST(rxt.should_close(gen, t0 + 60));

    ltfec::pipeline::RxClosedBlock closed{};
    BOOST_TEST(rxt.close_if_ready(gen, t0 + 60, closed));

    // Validate both missing frames were recovered
    BOOST_TEST(closed.was_recovered[1]);
    BOOST_TEST(closed.was_recovered[3]);
    for (size_t i = 0; i < d1.size(); ++i) {
        BOOST_TEST(u8(closed.data[1][i]) == u8(d1[i]));
        BOOST_TEST(u8(closed.data[3][i]) == u8(d3[i]));
    }
}

BOOST_AUTO_TEST_CASE(k3_recover_two_missing_with_only_two_parity_rows_present) {
    // N=4, K=3 — drop two data frames; only 2 parity rows arrive (still solvable).
    TxConfig txcfg{ .N = 4, .K = 3, .max_payload_len = 1300 };
    TxBlockAssembler tx(txcfg, /*gen_seed*/ 321u);

    const auto d0 = to_bytes("ABCD12");
    const auto d1 = to_bytes("EFGH34");
    const auto d2 = to_bytes("IJKL56");
    const auto d3 = to_bytes("MNOP78");

    std::vector<std::span<const std::byte>> data = {
        {d0.data(), d0.size()}, {d1.data(), d1.size()}, {d2.data(), d2.size()}, {d3.data(), d3.size()}
    };

    std::vector<std::vector<std::byte>> frames;
    BOOST_TEST(tx.assemble_block(data, frames));
    const std::uint32_t gen = tx.peek_next_gen() - 1;

    // Decode all
    std::vector<BaseHeader> H(frames.size());
    std::vector<uint8_t> HasP(frames.size(), 0);                 // <-- byte flags
    std::vector<ParitySubheader> PS(frames.size());
    std::vector<std::span<const std::byte>> PL(frames.size());
    std::uint32_t crc;
    for (size_t i = 0; i < frames.size(); ++i) {
        bool hasp = false;                                        // real bool
        BOOST_TEST(decode_frame(std::span<const std::byte>(frames[i].data(), frames[i].size()),
            H[i], hasp, PS[i], PL[i], crc));
        HasP[i] = hasp ? 1u : 0u;
    }

    // RX table
    RxBlockTable rxt({ .reorder_ms = 200, .fps = 30, .max_payload_len = 1300 }); // ensure close triggers at 60 ms (since min(60, 2×span)=60)
    const std::uint64_t t0 = 5000;

    // Ingest data #0 only; drop #1 and #2; keep #3
    BOOST_TEST(rxt.ingest(t0, H[0], static_cast<bool>(HasP[0]), PS[0], PL[0]));
    BOOST_TEST(rxt.ingest(t0, H[3], static_cast<bool>(HasP[3]), PS[3], PL[3]));

    // Ingest ONLY two parity frames (e.g., j=0 and j=2)
    BOOST_TEST(rxt.ingest(t0 + 1, H[4], static_cast<bool>(HasP[4]), PS[4], PL[4])); // parity j=0
    BOOST_TEST(rxt.ingest(t0 + 1, H[6], static_cast<bool>(HasP[6]), PS[6], PL[6])); // parity j=2 (skip j=1)

    // Close by time
    BOOST_TEST(rxt.should_close(gen, t0 + 60));
    ltfec::pipeline::RxClosedBlock closed{};
    BOOST_TEST(rxt.close_if_ready(gen, t0 + 60, closed));

    // We dropped #1 and #2; both should be recovered with two parity rows.
    BOOST_TEST(closed.was_recovered[1]);
    BOOST_TEST(closed.was_recovered[2]);

    for (size_t i = 0; i < d1.size(); ++i) {
        BOOST_TEST(u8(closed.data[1][i]) == u8(d1[i]));
        BOOST_TEST(u8(closed.data[2][i]) == u8(d2[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END()