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

    ltfec::transport::asio::net::io_context io;
    ltfec::transport::asio::UdpReceiver rx(io);

    if (auto ec = rx.open_and_bind(ep, mcast_if); ec) {
        std::cerr << "socket bind/join error: " << ec.message() << "\n"; return 3;
    }

    std::vector<std::byte> buf(2048);
    std::size_t n = 0;
    ltfec::transport::asio::udp::endpoint sender;
    if (auto ec = rx.recv_once(std::span<std::byte>(buf), n, sender); ec) {
        std::cerr << "recv error: " << ec.message() << "\n"; return 4;
    }

    std::cout << "received " << n << " bytes from " << sender.address().to_string()
        << ":" << sender.port() << "\n";
    return 0;
}