#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <chrono>
#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>

// NEW: use our protocol packers
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>

#include <ltfec/metrics/csv.h>
#include <ltfec/util/uuid.h>

using namespace ltfec::transport;
using namespace ltfec::protocol;

static void print_usage() {
    std::cout <<
        "fec_sender " << ltfec::version() << "\n"
        "Usage:\n"
        "  fec_sender --dest <ip:port> [--mcast-if <if_ip>] [--msg <text>] [--N <8>] [--K <1>]\n"
        "Notes:\n"
        "  - On Windows, multicast requires --mcast-if (IPv4 address).\n"
        "  - This build sends a single **data** frame (no parity yet).\n";
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
static int to_int_or(std::string const& s, int def) {
    if (s.empty()) return def;
    try { return std::stoi(s); }
    catch (...) { return def; }
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) if (std::string_view(argv[i]) == "--help") { print_usage(); return 0; }

    std::string dest_s, mcast_if, msg = "ltfec hello", N_s, K_s;
    if (!get_opt(argc, argv, "--dest", dest_s)) { std::cerr << "error: --dest required\n"; print_usage(); return 2; }
    get_opt(argc, argv, "--mcast-if", mcast_if);
    get_opt(argc, argv, "--msg", msg);
    get_opt(argc, argv, "--N", N_s);
    get_opt(argc, argv, "--K", K_s);
    const int N = to_int_or(N_s, 8);
    const int K = to_int_or(K_s, 1);

    Endpoint ep{};
    if (!parse_endpoint(dest_s, ep)) { std::cerr << "error: invalid --dest\n"; return 2; }
    if (ipv4_is_multicast(ep.addr) && mcast_if.empty()) {
        std::cerr << "error: multicast destination requires --mcast-if\n"; return 2;
    }

    // ---- Build a framed DATA packet (BaseHeader + payload + CRC) ----
    const auto payload_len = static_cast<std::uint16_t>(msg.size());
    std::vector<std::byte> payload(payload_len);
    for (size_t i = 0; i < msg.size(); ++i) payload[i] = std::byte{ static_cast<unsigned char>(msg[i]) };

    BaseHeader h{};
    h.version = k_protocol_version;
    h.flags1 = 0;
    h.flags2 = flags2_pack_parity_count_minus_one(static_cast<std::uint16_t>(K));
    h.fec_gen_id = static_cast<std::uint32_t>(now_ms()); // simple gen id for this demo
    h.seq_in_block = 0;                                    // data index 0 (single frame for now)
    h.data_count = static_cast<std::uint16_t>(N);
    h.parity_count = static_cast<std::uint16_t>(K);
    h.payload_len = payload_len;

    const std::size_t need = encoded_size(payload.size(), /*with_parity_subheader=*/false);
    std::vector<std::byte> frame(need);
    if (!encode_data_frame(std::span<std::byte>(frame), h, std::span<const std::byte>(payload))) {
        std::cerr << "encode_data_frame failed\n";
        return 3;
    }

    // ---- Metrics ----
    ltfec::metrics::CsvWriter m(1);
    const auto run_id = ltfec::util::uuid_v4();
    m.set_run_uuid(run_id);
    m.set_header({ "ts_ms","app","event","ip","port","bytes" });
    auto ts0 = now_ms();
    m.add_row({ std::to_string(ts0), "sender", "start",
               std::to_string((int)ep.addr.oct[0]) + "." + std::to_string((int)ep.addr.oct[1]) + "." +
               std::to_string((int)ep.addr.oct[2]) + "." + std::to_string((int)ep.addr.oct[3]),
               std::to_string(ep.port), "0" });

    // ---- Send via UDP ----
    ltfec::transport::asio::net::io_context io;
    ltfec::transport::asio::UdpSender sender(io);
    if (auto ec = sender.open_and_configure(ep, mcast_if); ec) {
        std::cerr << "socket config error: " << ec.message() << "\n";
        m.finish_with_summary(std::string("error: ") + ec.message());
        std::filesystem::create_directories("metrics");
        m.save_to_file(std::string("metrics\\sender_") + run_id + ".csv");
        return 4;
    }

    std::size_t n = 0;
    auto ec = sender.send(std::span<const std::byte>(frame), n);
    auto ts1 = now_ms();

    if (ec) {
        std::cerr << "send error: " << ec.message() << "\n";
        m.add_row({ std::to_string(ts1), "sender", "send_error", dest_s, std::to_string(ep.port), std::to_string(n) });
        m.finish_with_summary(std::string("error: ") + ec.message());
    }
    else {
        std::cout << "sent " << n << "B framed data to "
            << int(ep.addr.oct[0]) << "." << int(ep.addr.oct[1]) << "."
            << int(ep.addr.oct[2]) << "." << int(ep.addr.oct[3]) << ":" << ep.port
            << " (payload=" << payload_len << "B, frame=" << need << "B)"
            << (mcast_if.empty() ? "" : std::string(" via ") + mcast_if) << "\n";
        m.add_row({ std::to_string(ts1), "sender", "sent", dest_s, std::to_string(ep.port), std::to_string(n) });
        m.finish_with_summary("ok");
    }

    std::filesystem::create_directories("metrics");
    m.save_to_file(std::string("metrics\\sender_") + run_id + ".csv");
    return ec ? 5 : 0;
}