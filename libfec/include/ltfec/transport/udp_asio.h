#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <boost/asio.hpp>
#include <ltfec/transport/ip.h>

namespace ltfec::transport::asio {

    namespace net = boost::asio;
    using udp = net::ip::udp;

    inline udp::endpoint to_boost_endpoint(const Endpoint& ep) {
        boost::system::error_code ec;
        auto addr = net::ip::address_v4(
            (static_cast<unsigned>(ep.addr.oct[0]) << 24) |
            (static_cast<unsigned>(ep.addr.oct[1]) << 16) |
            (static_cast<unsigned>(ep.addr.oct[2]) << 8) |
            (static_cast<unsigned>(ep.addr.oct[3]) << 0));
        return udp::endpoint(addr, ep.port);
    }

    class UdpSender {
    public:
        explicit UdpSender(net::io_context& io) : io_(io), socket_(io) {}

        // mcast_if_ip: optional IPv4 address string for multicast egress
        boost::system::error_code open_and_configure(const Endpoint& dest, const std::string& mcast_if_ip);

        // Send one datagram (blocking) to dest configured above.
        boost::system::error_code send(std::span<const std::byte> payload, std::size_t& bytes_sent);

    private:
        net::io_context& io_;
        udp::socket socket_;
        udp::endpoint dest_;
    };

    class UdpReceiver {
    public:
        explicit UdpReceiver(net::io_context& io) : io_(io), socket_(io) {}

        // listen: if multicast, pass mcast_if_ip (IPv4) to join group on that interface.
        boost::system::error_code open_and_bind(const Endpoint& listen, const std::string& mcast_if_ip);

        // Receive one datagram (blocking). Returns sender endpoint and bytes received.
        boost::system::error_code recv_once(std::span<std::byte> buffer, std::size_t& bytes_recv, udp::endpoint& sender);

    private:
        net::io_context& io_;
        udp::socket socket_;
    };

} // namespace ltfec::transport::asio