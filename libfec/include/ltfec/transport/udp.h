#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ltfec::transport {

    // Placeholders for UDP sender/receiver construction. Asio wiring comes later.
    struct UdpBind {
        std::string local_address;   // e.g., "0.0.0.0"
        std::uint16_t local_port{};  // 0 = ephemeral
    };

    struct UdpEndpoint {
        std::string address;         // IPv4 dotted quad
        std::uint16_t port{};
    };

    struct McastConfig {
        bool enabled{ false };
        std::string outbound_if;     // required when enabled on Windows (DESIGN.md)
    };

    class UdpSenderConfig {
    public:
        UdpBind bind{};
        UdpEndpoint dest{};
        McastConfig mcast{};
        int dscp{ -1 }; // optional QoS; -1 = OS default
    };

    class UdpReceiverConfig {
    public:
        UdpBind bind{};
        McastConfig mcast{};
        int dscp{ -1 };
    };

} // namespace ltfec::transport