#include <ltfec/transport/udp_asio.h>
#include <boost/asio/ip/multicast.hpp>
#include <boost/system/error_code.hpp>

namespace ltfec::transport::asio {

    namespace net = boost::asio;
    using udp = net::ip::udp;

    static bool parse_ipv4_string(std::string_view s, net::ip::address_v4& out) {
        // We already have a parser in ip.h; reuse it to validate.
        ltfec::transport::Ipv4 tmp{};
        if (!ltfec::transport::parse_ipv4(s, tmp)) return false;
        unsigned v = (tmp.oct[0] << 24) | (tmp.oct[1] << 16) | (tmp.oct[2] << 8) | (tmp.oct[3] << 0);
        out = net::ip::address_v4(v);
        return true;
    }

    boost::system::error_code
        UdpSender::open_and_configure(const Endpoint& dest, const std::string& mcast_if_ip) {
        boost::system::error_code ec;
        socket_.open(udp::v4(), ec);
        if (ec) return ec;

        dest_ = to_boost_endpoint(dest);

        // If destination is multicast, set outbound interface
        if (ltfec::transport::ipv4_is_multicast(dest.addr)) {
            net::ip::address_v4 ifaddr;
            if (!parse_ipv4_string(mcast_if_ip, ifaddr)) {
                ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
                return ec;
            }
            socket_.set_option(net::ip::multicast::outbound_interface(ifaddr), ec);
            if (ec) return ec;

            // Optional: set TTL/hops if desired (default 1 is fine for local nets)
            // socket_.set_option(net::ip::multicast::hops(1), ec);
        }

        return ec; // success = default-constructed
    }

    boost::system::error_code
        UdpSender::send(std::span<const std::byte> payload, std::size_t& bytes_sent) {
        boost::system::error_code ec;
        bytes_sent = socket_.send_to(net::buffer(payload.data(), payload.size()), dest_, 0, ec);
        return ec;
    }

    boost::system::error_code
        UdpReceiver::open_and_bind(const Endpoint& listen, const std::string& mcast_if_ip) {
        boost::system::error_code ec;
        udp::endpoint lep = to_boost_endpoint(listen);

        socket_.open(udp::v4(), ec);
        if (ec) return ec;

        // For multicast receive allow multiple sockets on same addr:port
        socket_.set_option(net::ip::udp::socket::reuse_address(true), ec);
        if (ec) return ec;

        socket_.bind(lep, ec);
        if (ec) return ec;

        // If the listen addr is multicast, join the group on the specified interface.
        if (ltfec::transport::ipv4_is_multicast(listen.addr)) {
            net::ip::address_v4 grp(
                (listen.addr.oct[0] << 24) | (listen.addr.oct[1] << 16) |
                (listen.addr.oct[2] << 8) | (listen.addr.oct[3] << 0));
            net::ip::address_v4 ifaddr;
            if (!parse_ipv4_string(mcast_if_ip, ifaddr)) {
                ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
                return ec;
            }
            // Join specific group on specific interface.
            socket_.set_option(net::ip::multicast::join_group(grp, ifaddr), ec);
            if (ec) return ec;
        }
        return ec;
    }

    boost::system::error_code
        UdpReceiver::recv_once(std::span<std::byte> buffer, std::size_t& bytes_recv, udp::endpoint& sender) {
        boost::system::error_code ec;
        bytes_recv = socket_.receive_from(net::buffer(buffer.data(), buffer.size()), sender, 0, ec);
        return ec;
    }

} // namespace ltfec::transport::asio