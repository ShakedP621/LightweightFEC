// libfec/src/transport/udp_asio.cpp
#include <ltfec/transport/udp_asio.h>
#include <ltfec/transport/ip.h>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

namespace ltfec::transport::asio {

    namespace net = boost::asio;
    using udp = boost::asio::ip::udp;

    // NOTE: ip.h defines 'struct Ipv4' (lowercase v)
    static boost::asio::ip::address_v4 to_address_v4(const ltfec::transport::Ipv4& a) {
        boost::asio::ip::address_v4::bytes_type b{ { a.oct[0], a.oct[1], a.oct[2], a.oct[3] } };
        return boost::asio::ip::address_v4(b);
    }

    std::error_code UdpSender::open_and_configure(const Endpoint& dest, const std::string& mcast_if) {
        return open_and_configure(dest, mcast_if, std::nullopt, std::nullopt);
    }

    std::error_code UdpSender::open_and_configure(const Endpoint& dest,
        const std::string& mcast_if,
        std::optional<int> mcast_ttl,
        std::optional<bool> mcast_loopback)
    {
        boost::system::error_code bec;

        if (socket_.is_open()) {
            socket_.close(bec);
            bec = {};
        }

        socket_.open(udp::v4(), bec);
        if (bec) return std::error_code(bec.value(), std::generic_category());

        const auto addr = to_address_v4(dest.addr);
        dest_ep_ = udp::endpoint(addr, dest.port);

        const bool is_mcast = addr.is_multicast();
        if (is_mcast) {
            // Multicast requires interface
            if (mcast_if.empty()) {
                return std::make_error_code(std::errc::invalid_argument);
            }
            boost::system::error_code tmp;

            // Outbound interface
            const auto ifaddr = boost::asio::ip::make_address_v4(mcast_if, tmp);
            if (tmp) return std::error_code(tmp.value(), std::generic_category());
            socket_.set_option(boost::asio::ip::multicast::outbound_interface(ifaddr), tmp);
            if (tmp) return std::error_code(tmp.value(), std::generic_category());

            // TTL (hops)
            if (mcast_ttl.has_value()) {
                int ttl = *mcast_ttl;
                if (ttl < 0) ttl = 0;
                if (ttl > 255) ttl = 255;
                socket_.set_option(boost::asio::ip::multicast::hops(static_cast<int>(ttl)), tmp);
                if (tmp) return std::error_code(tmp.value(), std::generic_category());
            }

            // Loopback enable/disable
            if (mcast_loopback.has_value()) {
                socket_.set_option(boost::asio::ip::multicast::enable_loopback(*mcast_loopback), tmp);
                if (tmp) return std::error_code(tmp.value(), std::generic_category());
            }
        }

        return {};
    }

    std::error_code UdpSender::send(std::span<const std::byte> buf, std::size_t& sent) {
        boost::system::error_code bec;
        const auto n = socket_.send_to(net::buffer(buf.data(), buf.size()), dest_ep_, 0, bec);
        sent = static_cast<std::size_t>(n);
        if (bec) return std::error_code(bec.value(), std::generic_category());
        return {};
    }

    std::error_code UdpReceiver::open_and_bind(const Endpoint& listen_ep, const std::string& mcast_if) {
        boost::system::error_code bec;

        if (socket_.is_open()) {
            socket_.close(bec);
            bec = {};
        }

        socket_.open(udp::v4(), bec);
        if (bec) return std::error_code(bec.value(), std::generic_category());

        socket_.set_option(net::socket_base::reuse_address(true), bec);
        if (bec) return std::error_code(bec.value(), std::generic_category());

        const auto laddr = to_address_v4(listen_ep.addr);
        const bool is_mcast = laddr.is_multicast();

        // Bind: for multicast, bind to ANY on the port; for unicast, bind to the specific address
        udp::endpoint bind_ep(is_mcast ? udp::endpoint(udp::v4(), listen_ep.port)
            : udp::endpoint(laddr, listen_ep.port));
        socket_.bind(bind_ep, bec);
        if (bec) return std::error_code(bec.value(), std::generic_category());

        if (is_mcast) {
            if (mcast_if.empty()) {
                return std::make_error_code(std::errc::invalid_argument);
            }
            boost::system::error_code tmp;
            const auto group = laddr;
            const auto iface = boost::asio::ip::make_address_v4(mcast_if, tmp);
            if (tmp) return std::error_code(tmp.value(), std::generic_category());
            socket_.set_option(boost::asio::ip::multicast::join_group(group, iface), tmp);
            if (tmp) return std::error_code(tmp.value(), std::generic_category());
        }

        return {};
    }

    std::error_code UdpReceiver::recv_once(std::span<std::byte> buf, std::size_t& n, udp::endpoint& sender) {
        boost::system::error_code bec;
        const auto c = socket_.receive_from(net::buffer(buf.data(), buf.size()), sender, 0, bec);
        n = static_cast<std::size_t>(c);
        if (bec) return std::error_code(bec.value(), std::generic_category());
        return {};
    }

} // namespace ltfec::transport::asio