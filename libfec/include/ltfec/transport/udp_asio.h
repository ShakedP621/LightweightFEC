#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <span>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <ltfec/transport/ip.h>

namespace ltfec::transport::asio {

    namespace net = boost::asio;
    using udp = boost::asio::ip::udp;

    class UdpSender {
    public:
        explicit UdpSender(net::io_context& io) : socket_(io) {}

        // Legacy: no TTL/loopback controls
        std::error_code open_and_configure(const Endpoint& dest, const std::string& mcast_if);

        // New: with TTL and loopback
        std::error_code open_and_configure(const Endpoint& dest,
            const std::string& mcast_if,
            std::optional<int> mcast_ttl,
            std::optional<bool> mcast_loopback);

        std::error_code send(std::span<const std::byte> buf, std::size_t& sent);

    private:
        udp::socket   socket_;
        udp::endpoint dest_ep_;
    };

    class UdpReceiver {
    public:
        explicit UdpReceiver(net::io_context& io) : socket_(io) {}

        std::error_code open_and_bind(const Endpoint& listen_ep, const std::string& mcast_if);
        std::error_code recv_once(std::span<std::byte> buf, std::size_t& n, udp::endpoint& sender);

    private:
        udp::socket socket_;
    };

} // namespace ltfec::transport::asio