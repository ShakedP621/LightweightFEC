// apps/f/fec_sender/main.cpp
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cstddef>

#include <boost/program_options.hpp>

#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/pipeline/tx_block_assembler.h>
#include <ltfec/metrics/csv.h>
#include <ltfec/metrics/schema.h>
#include <ltfec/util/uuid.h>

using namespace ltfec::transport;
using namespace ltfec::protocol;
using namespace ltfec::pipeline;
namespace po = boost::program_options;

static inline std::uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static std::vector<std::byte> make_payload(std::string src, std::size_t L, std::uint16_t idx) {
    if (L == 0) return {};
    if (src.empty()) src = "x";
    std::vector<std::byte> out(L);
    for (size_t i = 0; i < L; ++i) {
        unsigned char ch = static_cast<unsigned char>(src[i % src.size()]);
        // vary by index a little so frames differ
        out[i] = std::byte{ static_cast<unsigned char>(ch ^ static_cast<unsigned char>((idx * 17u + i) & 0xFF)) };
    }
    return out;
}

int main(int argc, char** argv) {
    std::string dest_s;
    std::string mcast_if;
    std::string msg = "ltfec hello";
    int N = 8;
    int K = 1;
    int payload_len_cli = -1;
    int fps = 30;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help")
        ("version,v", "Show version")
        ("dest", po::value<std::string>(&dest_s), "Destination <ip:port> (required)")
        ("mcast-if", po::value<std::string>(&mcast_if)->default_value(""), "Multicast interface IPv4 (required when --dest is multicast)")
        ("mcast-ttl", po::value<int>()->default_value(1), "Multicast TTL (hops 0..255); used only for multicast dest")
        ("mcast-loopback", po::value<int>()->default_value(0), "Enable multicast loopback (0/1) for local testing")
        ("msg", po::value<std::string>(&msg)->default_value(msg), "Payload text (used if --payload-len < 0)")
        ("payload-len", po::value<int>(&payload_len_cli)->default_value(-1), "Payload length in bytes; if <0, use --msg length")
        ("N", po::value<int>(&N)->default_value(8), "Data frames per block")
        ("K", po::value<int>(&K)->default_value(1), "Parity frames per block (1=XOR, 2..4=GF(256))")
        ("fps", po::value<int>(&fps)->default_value(30), "Optional pacing for data frames (frames/second)")
        ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const std::exception& e) {
        std::cerr << "arg error: " << e.what() << "\n\n" << desc << "\n";
        return 2;
    }

    if (vm.count("help")) {
        std::cout << "fec_sender " << ltfec::version() << "\n" << desc << "\n";
        return 0;
    }
    if (vm.count("version")) {
        std::cout << ltfec::version() << "\n";
        return 0;
    }
    if (!vm.count("dest")) {
        std::cerr << "error: --dest is required\n\n" << desc << "\n";
        return 2;
    }
    if (N <= 0 || N > 255 || K < 0 || K > 4) {
        std::cerr << "error: invalid N/K (N:1..255, K:0..4)\n";
        return 2;
    }
    if (fps <= 0) fps = 30;

    Endpoint ep{};
    if (!parse_endpoint(dest_s, ep)) {
        std::cerr << "error: invalid --dest\n";
        return 2;
    }

    const bool dest_is_mcast = ipv4_is_multicast(ep.addr);
    if (dest_is_mcast && mcast_if.empty()) {
        std::cerr << "error: multicast destination requires --mcast-if\n";
        return 2;
    }

    // Resolve multicast options
    std::optional<int> mttl;
    std::optional<bool> mloop;
    if (dest_is_mcast) {
        int ttl_opt = vm["mcast-ttl"].as<int>();
        if (ttl_opt < 0) ttl_opt = 0;
        if (ttl_opt > 255) ttl_opt = 255;
        mttl = ttl_opt;

        int loop_opt = vm["mcast-loopback"].as<int>();
        mloop = (loop_opt != 0);
    }

    // ----- Build one block (N data + K parity) -----
    const std::size_t payload_len = (payload_len_cli >= 0) ? static_cast<std::size_t>(payload_len_cli)
        : static_cast<std::size_t>(msg.size());
    TxConfig txc{ .N = static_cast<std::uint16_t>(N),
                  .K = static_cast<std::uint16_t>(K),
                  .max_payload_len = 1300 };
    TxBlockAssembler tx(txc, /*gen_seed*/ static_cast<std::uint32_t>(now_ms() & 0xFFFFFFFFu));

    std::vector<std::vector<std::byte>> data_bytes(N);
    std::vector<std::span<const std::byte>> data_spans; data_spans.reserve(N);
    for (int i = 0; i < N; ++i) {
        data_bytes[i] = make_payload(msg, payload_len, static_cast<std::uint16_t>(i));
        data_spans.emplace_back(data_bytes[i].data(), data_bytes[i].size());
    }

    std::vector<std::vector<std::byte>> frames;
    if (!tx.assemble_block(data_spans, frames)) {
        std::cerr << "assemble_block failed\n";
        return 3;
    }
    const std::uint32_t gen = tx.peek_next_gen() - 1;

    // ---- Metrics (standard schema) ----
    ltfec::metrics::CsvWriter m(1);
    const auto run_id = ltfec::util::uuid_v4();
    m.set_run_uuid(run_id);
    m.set_header(ltfec::metrics::standard_header());

    auto ts0 = now_ms();
    m.add_row({
        std::to_string(ltfec::metrics::schema_version), run_id,
        std::to_string(ts0), "sender", "start",
        std::to_string((int)ep.addr.oct[0]) + "." + std::to_string((int)ep.addr.oct[1]) + "." +
        std::to_string((int)ep.addr.oct[2]) + "." + std::to_string((int)ep.addr.oct[3]),
        std::to_string(ep.port), "0"
        });

    // ---- UDP send ----
    ltfec::transport::asio::net::io_context io;
    ltfec::transport::asio::UdpSender sender(io);
    if (auto ec = sender.open_and_configure(ep, mcast_if, mttl, mloop); ec) {
        std::cerr << "socket config error: " << ec.message() << "\n";
        m.finish_with_summary(std::string("error: ") + ec.message());
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\sender_") + run_id + ".csv");
        return 4;
    }

    const int dt_ms = std::max(1, 1000 / fps);
    std::size_t total_sent = 0;

    for (size_t i = 0; i < frames.size(); ++i) {
        std::size_t n = 0;
        auto ec = sender.send(std::span<const std::byte>(frames[i].data(), frames[i].size()), n);
        auto ts = now_ms();
        total_sent += n;

        if (ec) {
            std::cerr << "send error on frame " << i << ": " << ec.message() << "\n";
            m.add_row({
                std::to_string(ltfec::metrics::schema_version), run_id,
                std::to_string(ts), "sender", "send_error",
                dest_s, std::to_string(ep.port), std::to_string(n)
                });
            m.finish_with_summary(std::string("error: ") + ec.message());
            break;
        }
        else {
            m.add_row({
                std::to_string(ltfec::metrics::schema_version), run_id,
                std::to_string(ts), "sender", (i < static_cast<size_t>(N) ? "sent_data" : "sent_parity"),
                dest_s, std::to_string(ep.port), std::to_string(n)
                });
        }

        // Pace only between DATA frames to roughly simulate fps
        if (i + 1 < static_cast<size_t>(N))
            std::this_thread::sleep_for(std::chrono::milliseconds(dt_ms));
    }

    std::cout << "sent block gen=" << gen << " with N=" << N << " K=" << K
        << " (payload=" << payload_len << "B each, frames=" << frames.size()
        << ", total=" << total_sent << "B)"
        << (dest_is_mcast ? " [multicast]" : "") << "\n";

    m.finish_with_summary("ok");
    std::filesystem::create_directories("metrics");
    m.save_to_file(std::string("metrics\\sender_") + run_id + ".csv");
    return 0;
}