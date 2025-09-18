#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/pipeline/rx_block_table.h>
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

BOOST_AUTO_TEST_SUITE(rx_recover_k1_suite)

BOOST_AUTO_TEST_CASE(recover_single_missing_on_close) {
    // Build N=3, K=1; then drop data frame #1.
    TxConfig txcfg{ .N = 3, .K = 1, .max_payload_len = 1300 };
    TxBlockAssembler tx(txcfg, /*gen_seed*/ 999u);

    const auto d0 = to_bytes("AAAAAA");
    const auto d1 = to_bytes("BBBBBB");
    const auto d2 = to_bytes("CCCCCC");
    std::vector<std::span<const std::byte>> data = {
        {d0.data(), d0.size()}, {d1.data(), d1.size()}, {d2.data(), d2.size()}
    };

    std::vector<std::vector<std::byte>> frames;
    BOOST_TEST(tx.assemble_block(data, frames));
    const std::uint32_t gen = tx.peek_next_gen() - 1;

    // Decode frames; simulate loss of frame #1 (index 1).
    BaseHeader h{}; bool hasp = false; ParitySubheader ps{}; std::span<const std::byte> pl; std::uint32_t crc = 0;

    // RX table
    RxBlockTable rxt({ .reorder_ms = 50, .fps = 30, .max_payload_len = 1300 });
    const std::uint64_t t0 = 1000;

    // Ingest data #0
    BOOST_TEST(decode_frame(std::span<const std::byte>(frames[0].data(), frames[0].size()), h, hasp, ps, pl, crc));
    BOOST_TEST(rxt.ingest(t0 + 0, h, hasp, ps, pl));

    // (skip #1)

    // Ingest data #2
    BOOST_TEST(decode_frame(std::span<const std::byte>(frames[2].data(), frames[2].size()), h, hasp, ps, pl, crc));
    BOOST_TEST(rxt.ingest(t0 + 1, h, hasp, ps, pl));

    // Ingest parity
    BOOST_TEST(decode_frame(std::span<const std::byte>(frames[3].data(), frames[3].size()), h, hasp, ps, pl, crc));
    BOOST_TEST(hasp);
    BOOST_TEST(rxt.ingest(t0 + 2, h, hasp, ps, pl));

    // Not closed yet (need time-based closure since one data is still missing)
    BOOST_TEST(!rxt.should_close(gen, t0 + 10));
    BOOST_TEST(rxt.should_close(gen, t0 + 60)); // min(60ms, 2*span) with N=3,fps=30 => 60ms

    // Close and extract
    RxClosedBlock closed{};
    BOOST_TEST(rxt.close_if_ready(gen, t0 + 60, closed));

    BOOST_TEST(closed.N == 3u);
    BOOST_TEST(closed.K == 1u);
    BOOST_TEST(closed.was_recovered.size() == 3u);
    BOOST_TEST(closed.was_recovered[1]); // index 1 was reconstructed

    // Validate recovered payload equals original d1
    BOOST_TEST(closed.data[1].size() == d1.size());
    for (size_t i = 0; i < d1.size(); ++i) {
        BOOST_TEST(u8(closed.data[1][i]) == u8(d1[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END()