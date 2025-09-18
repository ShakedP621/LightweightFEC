#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <ltfec/version.h>
#include <ltfec/transport/ip.h>
#include <ltfec/transport/udp_asio.h>

using namespace ltfec::transport;

static void print_usage() {
    std::cout <<
        "fec_sender " << ltfec::version() << "\n"
        "Usage:\n"
        "  fec_sender --dest <ip:port> [--mcast-if <if_ip>] [--msg <text>]\n"
        "Notes:\n"
        "  - On Windows, multicast requires --mcast-if (IPv4 address).\n";
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

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) if (std::string_view(argv[i]) == "--help") { print_usage(); return 0; }

    std::string dest_s, mcast_if, msg = "ltfec hello";
    if (!get_opt(argc, argv, "--dest", dest_s)) { std::cerr << "error: --dest required\n"; print_usage(); return 2; }
    get_opt(argc, argv, "--mcast-if", mcast_if);
    get_opt(argc, argv, "--msg", msg);

    Endpoint ep{};
    if (!parse_endpoint(dest_s, ep)) { std::cerr << "error: invalid --dest\n"; return 2; }
    if (ipv4_is_multicast(ep.addr) && mcast_if.empty()) {
        std::cerr << "error: multicast destination requires --mcast-if\n"; return 2;
    }

    ltfec::transport::asio::net::io_context io;
    ltfec::transport::asio::UdpSender sender(io);

    if (auto ec = sender.open_and_configure(ep, mcast_if); ec) {
        std::cerr << "socket config error: " << ec.message() << "\n"; return 3;
    }

    std::vector<std::byte> payload(msg.size());
    for (size_t i = 0; i < msg.size(); ++i) payload[i] = std::byte{ static_cast<unsigned char>(msg[i]) };

    std::size_t n = 0;
    if (auto ec = sender.send(std::span<const std::byte>(payload), n); ec) {
        std::cerr << "send error: " << ec.message() << "\n"; return 4;
    }

    std::cout << "sent " << n << " bytes to " << int(ep.addr.oct[0]) << "."
        << int(ep.addr.oct[1]) << "." << int(ep.addr.oct[2]) << "."
        << int(ep.addr.oct[3]) << ":" << ep.port
        << (mcast_if.empty() ? "" : std::string(" via ") + mcast_if) << "\n";
    return 0;
}