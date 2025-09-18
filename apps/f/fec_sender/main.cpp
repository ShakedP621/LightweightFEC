// apps/f/fec_sender/main.cpp
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>
#include <cstddef>

#include <boost/program_options.hpp>

#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>
#include <ltfec/protocol/frame.h>
#include <ltfec/protocol/frame_builder.h>
#include <ltfec/metrics/csv.h>
#include <ltfec/metrics/schema.h>
#include <ltfec/util/uuid.h>

using namespace ltfec::transport;
using namespace ltfec::protocol;
namespace po = boost::program_options;

static inline std::uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    std::string dest_s;
    std::string mcast_if;
    std::string msg = "ltfec hello";
    int N = 8;
    int K = 1;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help")
        ("version,v", "Show version")
        ("dest", po::value<std::string>(&dest_s), "Destination <ip:port> (required)")
        ("mcast-if", po::value<std::string>(&mcast_if)->default_value(""), "Multicast interface IPv4 (required when --dest is multicast)")
        ("mcast-ttl", po::value<int>()->default_value(1), "Multicast TTL (hops 0..255); used only for multicast dest")
        ("mcast-loopback", po::value<int>()->default_value(0), "Enable multicast loopback (0/1) for local testing")
        ("msg", po::value<std::string>(&msg)->default_value(msg), "Payload text")
        ("N", po::value<int>(&N)->default_value(8), "Data frames per block")
        ("K", po::value<int>(&K)->default_value(1), "Parity frames per block (1=XOR, 2..4=GF(256))");

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

    // Resolve multicast TTL/loopback only if multicast
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

    // Build a single framed DATA packet (demo send; full block assembly exists elsewhere)
    const auto payload_len = static_cast<std::uint16_t>(msg.size());
    std::vector<std::byte> payload(payload_len);
    for (size_t i = 0; i < msg.size(); ++i) payload[i] = std::byte{ static_cast<unsigned char>(msg[i]) };

    BaseHeader h{};
    h.version = k_protocol_version;
    h.flags1 = 0;
    h.flags2 = flags2_pack_parity_count_minus_one(static_cast<std::uint16_t>(K));
    h.fec_gen_id = static_cast<std::uint32_t>(now_ms());
    h.seq_in_block = 0;
    h.data_count = static_cast<std::uint16_t>(N);
    h.parity_count = static_cast<std::uint16_t>(K);
    h.payload_len = payload_len;

    const std::size_t need = encoded_size(payload.size(), /*parity*/false);
    std::vector<std::byte> frame(need);
    if (!encode_data_frame(std::span<std::byte>(frame), h, std::span<const std::byte>(payload))) {
        std::cerr << "encode_data_frame failed\n";
        return 3;
    }

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

    std::size_t n = 0;
    auto ec = sender.send(std::span<const std::byte>(frame), n);
    auto ts1 = now_ms();
    if (ec) {
        std::cerr << "send error: " << ec.message() << "\n";
        m.add_row({
            std::to_string(ltfec::metrics::schema_version), run_id,
            std::to_string(ts1), "sender", "send_error",
            dest_s, std::to_string(ep.port), std::to_string(n)
            });
        m.finish_with_summary(std::string("error: ") + ec.message());
    }
    else {
        std::cout << "sent " << n << "B framed data to "
            << int(ep.addr.oct[0]) << "." << int(ep.addr.oct[1]) << "."
            << int(ep.addr.oct[2]) << "." << int(ep.addr.oct[3]) << ":" << ep.port
            << " (payload=" << payload_len << "B, frame=" << need << "B)"
            << (dest_is_mcast ? " [multicast]" : "") << "\n";
        m.add_row({
            std::to_string(ltfec::metrics::schema_version), run_id,
            std::to_string(ts1), "sender", "sent",
            dest_s, std::to_string(ep.port), std::to_string(n)
            });
        m.finish_with_summary("ok");
    }

    std::filesystem::create_directories("metrics");
    m.save_to_file(std::string("metrics\\sender_") + run_id + ".csv");
    return ec ? 5 : 0;
}