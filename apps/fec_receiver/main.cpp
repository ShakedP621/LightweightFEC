#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

#include <boost/program_options.hpp>
#include <ltfec/metrics/schema.h>
#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/metrics/csv.h>
#include <ltfec/util/uuid.h>

using namespace ltfec::transport;
using namespace ltfec::protocol;
namespace po = boost::program_options;

static inline std::uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    std::string listen_s;
    std::string mcast_if;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help")
        ("version,v", "Show version")
        ("listen", po::value<std::string>(&listen_s), "Listen <ip:port> (required)")
        ("mcast-if", po::value<std::string>(&mcast_if)->default_value(""), "Multicast interface IPv4 (required when listen is multicast)");

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

    Endpoint ep{};
    if (!parse_endpoint(listen_s, ep)) { std::cerr << "error: invalid --listen\n"; return 2; }
    if (ipv4_is_multicast(ep.addr) && mcast_if.empty()) {
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

    // UDP receive (single datagram)
    ltfec::transport::asio::net::io_context io;
    ltfec::transport::asio::UdpReceiver rx(io);

    if (auto ec = rx.open_and_bind(ep, mcast_if); ec) {
        std::cerr << "socket bind/join error: " << ec.message() << "\n";
        m.finish_with_summary(std::string("error: ") + ec.message());
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
        return 3;
    }

    std::vector<std::byte> buf(4096);
    std::size_t n = 0;
    ltfec::transport::asio::udp::endpoint sender_ep;
    auto ec = rx.recv_once(std::span<std::byte>(buf), n, sender_ep);
    auto ts1 = now_ms();

    if (ec) {
        std::cerr << "recv error: " << ec.message() << "\n";
        m.add_row({
            std::to_string(ltfec::metrics::schema_version), run_id,
            std::to_string(ts1), "receiver", "recv_error",
            listen_s, std::to_string(ep.port), "0"
            });
        m.finish_with_summary(std::string("error: ") + ec.message());
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
        return 4;
    }

    // Parse the framed packet
    BaseHeader h{};
    bool has_parity = false;
    ParitySubheader ps{};
    std::span<const std::byte> payload;
    std::uint32_t crc = 0;

    const std::span<const std::byte> in(buf.data(), n);
    if (!decode_frame(in, h, has_parity, ps, payload, crc)) {
        std::cerr << "decode_frame failed (size " << n << ")\n";
        m.add_row({
            std::to_string(ltfec::metrics::schema_version), run_id,
            std::to_string(ts1), "receiver", "decode_error",
            listen_s, std::to_string(ep.port), std::to_string(n)
            });
        m.finish_with_summary("error: decode_frame failed");
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
        return 5;
    }

    const bool crc_ok = verify_payload_crc(payload, crc);
    std::cout << "received framed " << n << "B from " << sender_ep.address().to_string()
        << ":" << sender_ep.port()
        << " | ver=" << (unsigned)h.version
        << " gen=" << h.fec_gen_id
        << " seq=" << h.seq_in_block << "/" << h.data_count
        << " K=" << h.parity_count
        << " payload=" << h.payload_len
        << " parity?" << (has_parity ? "Y" : "N")
        << " crc=" << (crc_ok ? "ok" : "BAD")
        << "\n";

    m.add_row({
    std::to_string(ltfec::metrics::schema_version), run_id,
    std::to_string(ts1), "receiver", (crc_ok ? "received_ok" : "received_badcrc"),
    listen_s, std::to_string(ep.port), std::to_string(n)
        });

    m.finish_with_summary(crc_ok ? "ok" : "badcrc");

    std::filesystem::create_directories("metrics");
    m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
    return crc_ok ? 0 : 6;
}