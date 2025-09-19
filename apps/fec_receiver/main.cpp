// apps/fec_receiver/main.cpp
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <cstddef>

#include <boost/program_options.hpp>

#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/pipeline/rx_block_table.h>
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

int main(int argc, char** argv) {
    std::string listen_s;
    std::string mcast_if;
    int expect_blocks = 1;
    int reorder_ms = 200;
    int fps = 30;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help")
        ("version,v", "Show version")
        ("listen", po::value<std::string>(&listen_s), "Listen <ip:port> (required)")
        ("mcast-if", po::value<std::string>(&mcast_if)->default_value(""), "Multicast interface IPv4 (required for multicast)")
        ("expect-blocks", po::value<int>(&expect_blocks)->default_value(1), "Stop after closing this many blocks")
        ("reorder-ms", po::value<int>(&reorder_ms)->default_value(200), "Reorder window ms (>=50; 200 makes 60ms rule dominate)")
        ("fps", po::value<int>(&fps)->default_value(30), "Sender FPS hint (affects close rule 2×span)")
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
        std::cout << "fec_receiver " << ltfec::version() << "\n" << desc << "\n";
        return 0;
    }
    if (vm.count("version")) {
        std::cout << ltfec::version() << "\n";
        return 0;
    }
    if (!vm.count("listen")) {
        std::cerr << "error: --listen is required\n\n" << desc << "\n";
        return 2;
    }
    if (expect_blocks <= 0) expect_blocks = 1;
    if (reorder_ms < 50) reorder_ms = 50;
    if (fps <= 0) fps = 30;

    Endpoint ep{};
    if (!parse_endpoint(listen_s, ep)) { std::cerr << "error: invalid --listen\n"; return 2; }
    const bool is_mcast = ipv4_is_multicast(ep.addr);
    if (is_mcast && mcast_if.empty()) {
        std::cerr << "error: multicast listen requires --mcast-if\n"; return 2;
    }

    // ---- Metrics ----
    ltfec::metrics::CsvWriter m(1);
    const auto run_id = ltfec::util::uuid_v4();
    m.set_run_uuid(run_id);
    m.set_header(ltfec::metrics::standard_header());
    auto ts0 = now_ms();
    m.add_row({
        std::to_string(ltfec::metrics::schema_version), run_id,
        std::to_string(ts0), "receiver", "start",
        std::to_string((int)ep.addr.oct[0]) + "." + std::to_string((int)ep.addr.oct[1]) + "." +
        std::to_string((int)ep.addr.oct[2]) + "." + std::to_string((int)ep.addr.oct[3]),
        std::to_string(ep.port), "0"
        });

    // ---- UDP receive loop (multi-block) ----
    ltfec::transport::asio::net::io_context io;
    ltfec::transport::asio::UdpReceiver rx(io);

    if (auto ec = rx.open_and_bind(ep, mcast_if); ec) {
        std::cerr << "socket bind/join error: " << ec.message() << "\n";
        m.finish_with_summary(std::string("error: ") + ec.message());
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
        return 3;
    }

    RxBlockTable rxt({ .reorder_ms = static_cast<std::uint32_t>(reorder_ms),
                       .fps = static_cast<std::uint16_t>(fps),
                       .max_payload_len = 1300 });

    // Track gens so we can close multiple blocks
    int closed_blocks = 0;
    std::unordered_map<std::uint32_t, std::uint64_t> last_seen_ms;

    for (;;) {
        std::vector<std::byte> buf(4096);
        std::size_t n = 0;
        ltfec::transport::asio::udp::endpoint sender_ep;
        auto rc = rx.recv_once(std::span<std::byte>(buf.data(), buf.size()), n, sender_ep);
        const auto ts = now_ms();

        if (rc) {
            std::cerr << "recv error: " << rc.message() << "\n";
            m.add_row({
                std::to_string(ltfec::metrics::schema_version), run_id,
                std::to_string(ts), "receiver", "recv_error",
                listen_s, std::to_string(ep.port), "0"
                });
            m.finish_with_summary(std::string("error: ") + rc.message());
            break;
        }

        BaseHeader h{}; bool has_parity = false; ParitySubheader ps{};
        std::span<const std::byte> payload; std::uint32_t crc = 0;
        const std::span<const std::byte> in(buf.data(), n);

        if (!decode_frame(in, h, has_parity, ps, payload, crc) || !verify_payload_crc(payload, crc)) {
            std::cerr << "decode/crc error on packet size " << n << "\n";
            m.add_row({
                std::to_string(ltfec::metrics::schema_version), run_id,
                std::to_string(ts), "receiver", "decode_error",
                listen_s, std::to_string(ep.port), std::to_string(n)
                });
            continue;
        }

        rxt.ingest(ts, h, has_parity, ps, payload);
        last_seen_ms[h.fec_gen_id] = ts;

        // Optional per-frame log
        std::cout << "rx " << (has_parity ? "PAR" : "DAT")
            << " gen=" << h.fec_gen_id
            << " seq=" << h.seq_in_block << "/" << h.data_count
            << " K=" << h.parity_count
            << " payload=" << h.payload_len
            << " from " << sender_ep.address().to_string() << ":" << sender_ep.port()
            << "\n";

        // Try to close any gens that are ready (including this one)
        for (auto it = last_seen_ms.begin(); it != last_seen_ms.end(); /*in-loop*/) {
            const auto gen = it->first;
            const auto tlast = it->second;
            bool erased = false;

            if (rxt.should_close(gen, ts)) {
                RxClosedBlock closed{};
                if (rxt.close_if_ready(gen, ts, closed)) {
                    std::size_t recovered = 0, present = 0;
                    for (std::uint16_t i = 0; i < closed.N; ++i) {
                        if (!closed.data[i].empty()) ++present;
                        if (closed.was_recovered[i]) ++recovered;
                    }
                    std::cout << "block CLOSED gen=" << closed.gen
                        << " N=" << closed.N << " K=" << closed.K
                        << " payload=" << closed.payload_len
                        << " present=" << present
                        << " recovered=" << recovered << "\n";

                    m.add_row({
                        std::to_string(ltfec::metrics::schema_version), run_id,
                        std::to_string(ts), "receiver", "block_closed",
                        listen_s, std::to_string(ep.port),
                        std::to_string(present)
                        });

                    it = last_seen_ms.erase(it);
                    erased = true;
                    ++closed_blocks;
                    if (closed_blocks >= expect_blocks) {
                        m.finish_with_summary("ok");
                        goto end;
                    }
                }
            }

            if (!erased) ++it;
            (void)tlast; // reserved for future timeouts logic
        }
    }

end:
    std::filesystem::create_directories("metrics");
    m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
    return 0;
}