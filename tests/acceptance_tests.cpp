#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/pipeline/rx_block_table.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/sim/loss.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <cstring>

using namespace ltfec::pipeline;
using namespace ltfec::protocol;
using namespace ltfec::sim;

static bool env_flag(const char* name) {
#ifdef _WIN32
    char* buf = nullptr; size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) != 0 || !buf) return false;
    std::string v(buf); free(buf);
#else
    const char* c = std::getenv(name);
    if (!c) return false;
    std::string v(c);
#endif
    // trim + lower
    auto trim = [](std::string& s) {
        auto issp = [](unsigned char ch) { return std::isspace(ch) != 0; };
        while (!s.empty() && issp(s.front())) s.erase(s.begin());
        while (!s.empty() && issp(s.back()))  s.pop_back();
        for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        };
    trim(v);
    return (v == "1" || v == "true" || v == "yes" || v == "on");
}

struct Arrival {
    std::uint64_t t;
    // Which block and which position in its frame vector (0..N-1 data, then N..N+K-1 parity)
    std::uint32_t block_idx;
    std::uint16_t frame_idx_in_block;
    bool operator<(const Arrival& o) const { return t < o.t; }
};

static std::vector<std::byte> fill_payload(std::uint16_t idx, std::size_t L) {
    std::vector<std::byte> v(L);
    const auto base = static_cast<unsigned char>((idx * 37u) % 200u + 20u);
    for (size_t i = 0; i < L; ++i) v[i] = std::byte{ static_cast<unsigned char>(base ^ static_cast<unsigned char>(i)) };
    return v;
}

struct RunResult {
    double raw_loss = 0.0;
    double effective_loss = 0.0;
};

static RunResult run_acceptance_once(double p_loss,
    std::uint32_t seconds,
    std::uint32_t seed,
    std::uint16_t N = 8,
    std::uint16_t K = 1,
    std::uint16_t fps = 30,
    std::uint32_t jitter_ms = 50,
    std::size_t payload_len = 64)
{
    const std::uint64_t dt_ms = 1000u / fps;                          // 33 ms
    const std::uint32_t blocks = static_cast<std::uint32_t>((seconds * fps + (N - 1)) / N);
    const std::uint32_t total_data = blocks * N;

    // --- Build all blocks via TX
    TxConfig txc{ .N = N, .K = K, .max_payload_len = 1300 };
    TxBlockAssembler tx(txc, /*gen_seed*/ 2025u);

    std::vector<std::vector<std::vector<std::byte>>> frames_by_block(blocks);
    std::vector<std::uint32_t> gen_ids(blocks);
    for (std::uint32_t b = 0; b < blocks; ++b) {
        std::vector<std::vector<std::byte>> data_bytes(N);
        std::vector<std::span<const std::byte>> data_spans; data_spans.reserve(N);
        for (std::uint16_t i = 0; i < N; ++i) {
            data_bytes[i] = fill_payload(static_cast<std::uint16_t>(b * N + i), payload_len);
            data_spans.emplace_back(data_bytes[i].data(), data_bytes[i].size());
        }
        BOOST_REQUIRE(tx.assemble_block(data_spans, frames_by_block[b]));
        gen_ids[b] = tx.peek_next_gen() - 1;
    }

    // Decode all frames once for reuse
    struct Decoded {
        BaseHeader h; bool hasp; ParitySubheader ps; std::span<const std::byte> pl; std::uint32_t crc;
        // keep backing storage for span
        std::vector<std::byte> storage;
    };
    std::vector<std::vector<Decoded>> dec(blocks);

    for (std::uint32_t b = 0; b < blocks; ++b) {
        auto& vec = dec[b];
        vec.resize(frames_by_block[b].size());
        for (size_t i = 0; i < frames_by_block[b].size(); ++i) {
            vec[i].storage = frames_by_block[b][i]; // own bytes
            auto in = std::span<const std::byte>(vec[i].storage.data(), vec[i].storage.size());
            vec[i].hasp = false;
            BOOST_REQUIRE(decode_frame(in, vec[i].h, vec[i].hasp, vec[i].ps, vec[i].pl, vec[i].crc));
            BOOST_REQUIRE(verify_payload_crc(vec[i].pl, vec[i].crc));
        }
    }

    // --- Channel: Bernoulli loss + uniform jitter on both data and parity
    XorShift32 rng(seed);
    BernoulliLoss bern{ p_loss };

    std::vector<Arrival> arrivals;
    arrivals.reserve(blocks * (N + K));

    std::uint64_t T0 = 1'000'000; // arbitrary start
    const std::uint64_t block_span_ms = static_cast<std::uint64_t>(N) * dt_ms;

    std::uint32_t dropped_data = 0;

    for (std::uint32_t b = 0; b < blocks; ++b) {
        const std::uint64_t base = T0 + static_cast<std::uint64_t>(b) * block_span_ms;
        const auto& vec = dec[b];

        // Data 0..N-1
        for (std::uint16_t i = 0; i < N; ++i) {
            const bool drop = bern.drop(rng);
            if (drop) { ++dropped_data; continue; }
            const std::uint64_t jitter = jitter_uniform_ms(rng, jitter_ms);
            arrivals.push_back(Arrival{ base + static_cast<std::uint64_t>(i) * dt_ms + jitter, b, i });
        }
        // Parity N..N+K-1
        for (std::uint16_t j = 0; j < K; ++j) {
            const bool drop = bern.drop(rng);
            if (drop) continue;
            const std::uint64_t jitter = jitter_uniform_ms(rng, jitter_ms);
            arrivals.push_back(Arrival{ base + static_cast<std::uint64_t>(N) * dt_ms + jitter, b, static_cast<std::uint16_t>(N + j) });
        }
    }
    std::sort(arrivals.begin(), arrivals.end());

    // --- RX ingest in arrival order
    RxBlockTable rxt({ .reorder_ms = 200, .fps = 30, .max_payload_len = 1300 }); // ensure 60ms dominates
    std::uint64_t last_t = T0;

    for (const auto& a : arrivals) {
        const auto& d = dec[a.block_idx][a.frame_idx_in_block];
        BOOST_REQUIRE(rxt.ingest(a.t, d.h, d.hasp, d.ps, d.pl));
        last_t = a.t;
    }

    // --- Close all pending blocks
    std::uint32_t unrecovered_data = 0;
    const std::uint64_t final_t = last_t + 1000; // enough to pass any timers

    for (std::uint32_t b = 0; b < blocks; ++b) {
        const auto gen = gen_ids[b];
        if (!rxt.should_close(gen, final_t)) {
            // force-close via time
            // (should not happen with 1000ms margin, but be defensive)
        }
        RxClosedBlock closed{};
        if (rxt.close_if_ready(gen, final_t, closed)) {
            for (std::uint16_t i = 0; i < N; ++i) {
                if (closed.data[i].size() != payload_len) ++unrecovered_data;
            }
        }
        else {
            // If somehow not closable, count all its data as lost (ultra defensive)
            unrecovered_data += N;
        }
    }

    RunResult r{};
    r.raw_loss = static_cast<double>(dropped_data) / static_cast<double>(total_data);
    r.effective_loss = static_cast<double>(unrecovered_data) / static_cast<double>(total_data);
    return r;
}

static void assert_acceptance(double p, double eff, double raw) {
    // Allow a tiny slack for randomness in short runs.
    const double factor = eff / (raw > 0.0 ? raw : 1.0);
    BOOST_TEST(raw > 0.0);
    // target ≤ 0.2×; allow +0.02 absolute slack in quick mode, tighter in full.
    const bool full = env_flag("LTFEC_ACCEPT_FULL");
    const double tol = full ? 0.01 : 0.02;
    BOOST_TEST(factor <= 0.20 + tol);
}

BOOST_AUTO_TEST_SUITE(acceptance_suite)

BOOST_AUTO_TEST_CASE(acceptance_quick_or_full_k1) {
    const bool full = env_flag("LTFEC_ACCEPT_FULL");
    const std::uint32_t seconds = full ? 120u : 15u;

    const double ps[] = { 0.01, 0.03, 0.05 };
    const std::uint32_t seeds[] = { 123u, 456u, 789u }; // deterministic variety

    for (int i = 0; i < 3; ++i) {
        const auto res = run_acceptance_once(ps[i], seconds, seeds[i], /*N=*/8, /*K=*/1, /*fps=*/30, /*J=*/50, /*L=*/64);
        // std::cout could be added, but keep test quiet
        assert_acceptance(ps[i], res.effective_loss, res.raw_loss);
    }
}

BOOST_AUTO_TEST_SUITE_END()