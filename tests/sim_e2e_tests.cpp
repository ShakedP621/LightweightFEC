#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/pipeline/rx_block_table.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/sim/loss.h>   // jitter_uniform_ms, XorShift32
#include <vector>
#include <string>
#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace ltfec::pipeline;
using namespace ltfec::protocol;
using namespace ltfec::sim;

static std::vector<std::byte> fill_pattern(std::uint16_t idx, std::size_t L) {
    std::vector<std::byte> v(L);
    const auto b = static_cast<unsigned char>(0x20 + (idx % 90)); // printable-ish
    for (size_t i = 0; i < L; ++i) v[i] = std::byte{ b };
    return v;
}
static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

// Pick exactly K distinct data indices in [0..N-1] to drop, deterministically from RNG.
static std::vector<std::uint16_t> pick_losses(std::uint16_t N, std::uint16_t K, XorShift32& rng) {
    std::vector<std::uint16_t> all(N);
    for (std::uint16_t i = 0; i < N; ++i) all[i] = i;
    // Fisher-Yates shuffle driven by RNG; only need first K
    for (int i = static_cast<int>(N) - 1; i > 0; --i) {
        const std::uint32_t r = rng.next_u32();
        const int j = static_cast<int>(r % (i + 1));
        std::swap(all[i], all[j]);
    }
    all.resize(K);
    std::sort(all.begin(), all.end());
    return all;
}

struct Arrival {
    std::uint64_t t;
    size_t frame_idx; // index into frames vector
    bool operator<(const Arrival& o) const { return t < o.t; }
};

static void e2e_once(std::uint16_t N, std::uint16_t K, std::size_t L, std::uint32_t seed) {
    // ----- TX: assemble one block -----
    TxConfig txc{ .N = N, .K = K, .max_payload_len = 1300 };
    TxBlockAssembler tx(txc, /*gen_seed*/ 555u);

    std::vector<std::vector<std::byte>> data_bytes(N);
    std::vector<std::span<const std::byte>> data_spans;
    data_spans.reserve(N);
    for (std::uint16_t i = 0; i < N; ++i) {
        data_bytes[i] = fill_pattern(i, L);
        data_spans.emplace_back(data_bytes[i].data(), data_bytes[i].size());
    }

    std::vector<std::vector<std::byte>> frames;
    BOOST_REQUIRE(tx.assemble_block(data_spans, frames)); // frames.size() == N+K
    const std::uint32_t gen = tx.peek_next_gen() - 1;

    // Decode all frames to (H, has_parity, PS, PL)
    std::vector<BaseHeader> H(frames.size());
    std::vector<uint8_t> HasP(frames.size(), 0);
    std::vector<ParitySubheader> PS(frames.size());
    std::vector<std::span<const std::byte>> PL(frames.size());
    std::uint32_t crc = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
        bool hasp = false;
        BOOST_REQUIRE(decode_frame(std::span<const std::byte>(frames[i].data(), frames[i].size()),
            H[i], hasp, PS[i], PL[i], crc));
        HasP[i] = hasp ? 1u : 0u;
        BOOST_REQUIRE(verify_payload_crc(PL[i], crc));
    }

    // ----- Channel: deterministic losses + jitter reordering -----
    XorShift32 rng(seed);
    const std::uint32_t J = 50; // jitter in ms, uniform [0..J]
    const auto drops = pick_losses(N, K, rng); // drop exactly K data frames

    auto is_dropped_data = [&](std::uint16_t seq_in_block)->bool {
        // Only drop data frames by seq index; keep all parity frames
        return std::binary_search(drops.begin(), drops.end(), seq_in_block);
        };

    const std::uint64_t t0 = 10'000;
    std::vector<Arrival> arrivals;

    for (size_t i = 0; i < frames.size(); ++i) {
        if (!HasP[i]) {
            // data: H[i].seq_in_block in [0..N-1]
            if (is_dropped_data(H[i].seq_in_block)) continue; // simulate loss
        }
        const auto dt = jitter_uniform_ms(rng, J);
        arrivals.push_back(Arrival{ t0 + dt, i });
    }
    std::sort(arrivals.begin(), arrivals.end());

    // ----- RX ingest in arrival order -----
    RxBlockTable rxt({ .reorder_ms = 200, .fps = 30, .max_payload_len = 1300 }); // ensure 60ms rule dominates
    for (const auto& a : arrivals) {
        BOOST_REQUIRE(rxt.ingest(a.t, H[a.frame_idx], static_cast<bool>(HasP[a.frame_idx]), PS[a.frame_idx], PL[a.frame_idx]));
    }

    // Close after jitter window + 60ms (per policy)
    const std::uint64_t t_close = t0 + J + 60;
    BOOST_REQUIRE(rxt.should_close(gen, t_close));

    RxClosedBlock closed{};
    BOOST_REQUIRE(rxt.close_if_ready(gen, t_close, closed));

    // ----- Validate recovery: exactly K lost data indices are recovered, and payloads match -----
    BOOST_TEST(closed.N == N);
    BOOST_TEST(closed.K == K);
    for (std::uint16_t i = 0; i < N; ++i) {
        const bool was_dropped = std::binary_search(drops.begin(), drops.end(), i);
        if (was_dropped) {
            BOOST_TEST(closed.was_recovered[i]);
        }
        else {
            BOOST_TEST(!closed.was_recovered[i]);
        }
        BOOST_REQUIRE_EQUAL(closed.data[i].size(), L);
        for (size_t b = 0; b < L; ++b) {
            BOOST_TEST(u8(closed.data[i][b]) == u8(data_bytes[i][b]));
        }
    }
}

BOOST_AUTO_TEST_SUITE(sim_e2e_suite)

BOOST_AUTO_TEST_CASE(e2e_k1_single_erasure) {
    e2e_once(/*N=*/8, /*K=*/1, /*L=*/64, /*seed=*/123u);
}

BOOST_AUTO_TEST_CASE(e2e_k2_double_erasure) {
    e2e_once(/*N=*/8, /*K=*/2, /*L=*/64, /*seed=*/456u);
}

BOOST_AUTO_TEST_SUITE_END()