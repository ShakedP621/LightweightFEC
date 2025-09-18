#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/pipeline/rx_block_table.h>
#include <ltfec/protocol/frame_builder.h>

using namespace ltfec::pipeline;
using namespace ltfec::protocol;

static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

BOOST_AUTO_TEST_SUITE(rx_block_table_suite)

BOOST_AUTO_TEST_CASE(immediate_close_when_parity_and_all_data_present) {
    // Build a block N=3, K=1
    TxConfig txcfg{ .N = 3, .K = 1, .max_payload_len = 1300 };
    TxBlockAssembler tx(txcfg, /*gen_seed*/ 777u);

    // Three equal payloads
    std::vector<std::byte> a = { std::byte{'A'},std::byte{'A'},std::byte{'A'},std::byte{'A'} };
    std::vector<std::byte> b = { std::byte{'B'},std::byte{'B'},std::byte{'B'},std::byte{'B'} };
    std::vector<std::byte> c = { std::byte{'C'},std::byte{'C'},std::byte{'C'},std::byte{'C'} };
    std::vector<std::span<const std::byte>> data = {
        {a.data(), a.size()}, {b.data(), b.size()}, {c.data(), c.size()}
    };

    std::vector<std::vector<std::byte>> frames;
    BOOST_TEST(tx.assemble_block(data, frames));
    BOOST_TEST(frames.size() == 4u);

    const std::uint32_t gen = tx.peek_next_gen() - 1;

    RxBlockTable rxt({ .reorder_ms = 50, .fps = 30, .max_payload_len = 1300 });

    // Ingest all data frames first (no close yet).
    std::uint64_t t0 = 1000;
    for (size_t i = 0; i < 3; ++i) {
        BaseHeader h{}; bool hasp = false; ParitySubheader ps{}; std::span<const std::byte> pl; std::uint32_t crc = 0;
        BOOST_TEST(decode_frame(std::span<const std::byte>(frames[i].data(), frames[i].size()), h, hasp, ps, pl, crc));
        BOOST_TEST(rxt.ingest(t0 + static_cast<std::uint64_t>(i), h, hasp, ps, pl));
        BOOST_TEST(!rxt.should_close(gen, t0 + static_cast<std::uint64_t>(i))); // still missing parity
    }

    // Ingest parity -> close immediately (have_parity && have_all_data)
    {
        BaseHeader h{}; bool hasp = false; ParitySubheader ps{}; std::span<const std::byte> pl; std::uint32_t crc = 0;
        BOOST_TEST(decode_frame(std::span<const std::byte>(frames[3].data(), frames[3].size()), h, hasp, ps, pl, crc));
        BOOST_TEST(hasp);
        BOOST_TEST(rxt.ingest(t0 + 10, h, hasp, ps, pl));
        BOOST_TEST(rxt.should_close(gen, t0 + 10));
    }

    // Snapshot sanity
    auto snap = rxt.snapshot(gen);
    BOOST_TEST(snap.has_value());
    BOOST_TEST(snap->N == 3u);
    BOOST_TEST(snap->K == 1u);
    BOOST_TEST(snap->data_seen == 3u);
    BOOST_TEST(snap->parity_seen == 1u);
    BOOST_TEST(snap->have_all_data);
    BOOST_TEST(snap->have_any_parity);
}

BOOST_AUTO_TEST_CASE(close_by_reorder_ms_if_earlier) {
    // Build N=2, K=0 (no parity) so time policy drives closure
    TxConfig txcfg{ .N = 2, .K = 0, .max_payload_len = 1300 };
    TxBlockAssembler tx(txcfg, /*gen_seed*/ 111u);

    std::vector<std::byte> a = { std::byte{'x'} };
    std::vector<std::byte> b = { std::byte{'y'} };
    std::vector<std::span<const std::byte>> data = { {a.data(), a.size()},{b.data(), b.size()} };

    std::vector<std::vector<std::byte>> frames;
    BOOST_TEST(tx.assemble_block(data, frames));
    const std::uint32_t gen = tx.peek_next_gen() - 1;

    RxBlockTable rxt({ .reorder_ms = 50, .fps = 30, .max_payload_len = 1300 });
    const std::uint64_t t0 = 2000;

    for (size_t i = 0; i < data.size(); ++i) {
        BaseHeader h{}; bool hasp = false; ParitySubheader ps{}; std::span<const std::byte> pl; std::uint32_t crc = 0;
        BOOST_TEST(decode_frame(std::span<const std::byte>(frames[i].data(), frames[i].size()), h, hasp, ps, pl, crc));
        BOOST_TEST(rxt.ingest(t0, h, hasp, ps, pl));
    }
    BOOST_TEST(!rxt.should_close(gen, t0 + 49));
    BOOST_TEST(rxt.should_close(gen, t0 + 50));
}

BOOST_AUTO_TEST_SUITE_END()