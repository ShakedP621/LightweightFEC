#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ltfec::transport {

    struct Ipv4 {
        std::array<std::uint8_t, 4> oct{};
    };

    struct Endpoint {
        Ipv4 addr{};
        std::uint16_t port{};
    };

    inline bool ipv4_is_multicast(const Ipv4& a) noexcept {
        const auto o0 = a.oct[0];
        return o0 >= 224u && o0 <= 239u; // 224.0.0.0/4
    }

    inline bool parse_uint(std::string_view s, unsigned& out, unsigned maxv) {
        if (s.empty()) return false;
        unsigned v = 0;
        auto* b = s.data();
        auto* e = s.data() + s.size();
        auto res = std::from_chars(b, e, v, 10);
        if (res.ec != std::errc() || res.ptr != e) return false;
        if (v > maxv) return false;
        out = v;
        return true;
    }

    inline bool parse_ipv4(std::string_view s, Ipv4& out) {
        // Expect "a.b.c.d", each 0..255, no spaces.
        std::array<unsigned, 4> tmp{};
        size_t pos = 0, idx = 0;
        while (idx < 4) {
            size_t dot = s.find('.', pos);
            std::string_view tok = (dot == std::string_view::npos) ? s.substr(pos) : s.substr(pos, dot - pos);
            if (tok.empty()) return false;
            unsigned v = 0;
            if (!parse_uint(tok, v, 255)) return false;
            tmp[idx++] = v;
            if (dot == std::string_view::npos) break;
            pos = dot + 1;
        }
        if (idx != 4) return false;
        for (size_t i = 0; i < 4; ++i) out.oct[i] = static_cast<std::uint8_t>(tmp[i]);
        return true;
    }

    inline bool parse_endpoint(std::string_view s, Endpoint& out) {
        // Expect "a.b.c.d:port" with port in [1,65535]
        size_t colon = s.rfind(':');
        if (colon == std::string_view::npos) return false;
        std::string_view ip = s.substr(0, colon);
        std::string_view ps = s.substr(colon + 1);
        if (ip.empty() || ps.empty()) return false;
        if (!parse_ipv4(ip, out.addr)) return false;
        unsigned p = 0;
        if (!parse_uint(ps, p, 65535u) || p == 0u) return false;
        out.port = static_cast<std::uint16_t>(p);
        return true;
    }

} // namespace ltfec::transport