#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <chrono>
#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>

// NEW: protocol decode
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>

#include <ltfec/metrics/csv.h>
#include <ltfec/util/uuid.h>

using namespace ltfec::transport;
using namespace ltfec::protocol;

static void print_usage() {
    std::cout <<
        "fec_receiver " << ltfec::version() << "\n"
        "Usage:\n"
        "  fec_receiver --listen <ip:port> [--mcast-if <if_ip>]\n"
        "Notes:\n"
        "  - On Windows, multicast listen requires --mcast-if (IPv4 address).\n";
}

static bool get_opt(int argc, char** argv, std::string_view name, std::string& out) {
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == name && i + 1 < argc) { out = argv[i + 1]; return true; }
        if (a.rfind(name, 0) == 0 && a.size() > name.size() && a[name.size()] == '=') {
            out = std::string(a.substr(name.size() + 1));
            return true;
        }
    }
    return false;
}

static inline std::uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) if (std::string_view(argv[i]) == "--help") { print_usage(); return 0; }

    std::string listen_s, mcast_if;
    if (!get_opt(argc, argv, "--listen", listen_s)) { std::cerr << "error: --listen required\n"; print_usage(); return 2; }
    get_opt(argc, argv, "--mcast-if", mcast_if);

    Endpoint ep{};
    if (!parse_endpoint(listen_s, ep)) { std::cerr << "error: invalid --listen\n"; return 2; }
    if (ipv4_is_multicast(ep.addr) && mcast_if.empty()) {
        std::cerr << "error: multicast listen requires --mcast-if\n"; return 2;
    }

    // ---- Metrics ----
    ltfec::metrics::CsvWriter m(1);
    const auto run_id = ltfec::util::uuid_v4();
    m.set_run_uuid(run_id);
    m.set_header({ "ts_ms","app","event","ip","port","bytes" });

    auto ts0 = now_ms();
    m.add_row({ std::to_string(ts0), "receiver", "start",
               std::to_string((int)ep.addr.oct[0]) + "." + std::to_string((int)ep.addr.oct[1]) + "." +
               std::to_string((int)ep.addr.oct[2]) + "." + std::to_string((int)ep.addr.oct[3]),
               std::to_string(ep.port), "0" });

    // ---- UDP receive ----
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
        m.add_row({ std::to_string(ts1), "receiver", "recv_error", listen_s, std::to_string(ep.port), "0" });
        m.finish_with_summary(std::string("error: ") + ec.message());
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
        return 4;
    }

    // ---- Parse the framed packet ----
    BaseHeader h{};
    bool has_parity = false;
    ParitySubheader ps{};
    std::span<const std::byte> payload;
    std::uint32_t crc = 0;

    const std::span<const std::byte> in(buf.data(), n);
    if (!decode_frame(in, h, has_parity, ps, payload, crc)) {
        std::cerr << "decode_frame failed (size " << n << ")\n";
        m.add_row({ std::to_string(ts1), "receiver", "decode_error", listen_s, std::to_string(ep.port), std::to_string(n) });
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

    m.add_row({ std::to_string(ts1), "receiver", (crc_ok ? "received_ok" : "received_badcrc"),
               listen_s, std::to_string(ep.port), std::to_string(n) });
    m.finish_with_summary(crc_ok ? "ok" : "badcrc");

    std::filesystem::create_directories("metrics");
    m.save_to_file(std::string("metrics\\receiver_") + run_id + ".csv");
    return crc_ok ? 0 : 6;
}