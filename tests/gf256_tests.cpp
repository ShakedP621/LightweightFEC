#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/fec_core/gf256.h>
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

using namespace ltfec::fec_core;

// Local reference helpers mirroring the encoder math (same α, poly)
namespace {
    static constexpr std::uint16_t kPoly = 0x11D;
    struct Tbl {
        std::vector<std::uint8_t> exp, log;
        Tbl() : exp(512), log(256) {
            std::uint16_t x = 1;
            for (int i = 0; i < 255; ++i) {
                exp[i] = static_cast<std::uint8_t>(x);
                log[exp[i]] = static_cast<std::uint8_t>(i);
                x <<= 1;
                if (x & 0x100) x ^= kPoly;
            }
            for (int i = 255; i < 512; ++i) exp[i] = exp[i - 255];
        }
        std::uint8_t mul(std::uint8_t a, std::uint8_t b) const {
            if (a == 0 || b == 0) return 0;
            return exp[log[a] + log[b]];
        }
        std::uint8_t pow_a(unsigned e) const { return exp[e % 255]; }
    };

    static Tbl& T() { static Tbl t; return t; }

    static std::vector<std::byte> to_bytes(const std::string& s) {
        std::vector<std::byte> v(s.size());
        for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
        return v;
    }
    static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

} // namespace

BOOST_AUTO_TEST_SUITE(gf256_suite)

BOOST_AUTO_TEST_CASE(encode_K2_matches_reference) {
    // Three data frames, frame_len 8
    const auto d0 = to_bytes("ABCDEFGH");
    const auto d1 = to_bytes("ijklmnop");
    const auto d2 = to_bytes("QRSTUVWX");
    const size_t L = d0.size();

    const std::byte* data[3] = { d0.data(), d1.data(), d2.data() };
    std::vector<std::byte> p0(L), p1(L);
    std::byte* parity[2] = { p0.data(), p1.data() };

    gf256_encode(std::span<const std::byte* const>(data, 3), L,
        std::span<std::byte*>(parity, 2));

    // Reference computation: p_j = Σ_d α^(j*d) * data_d
    for (size_t i = 0; i < L; ++i) {
        // j = 0 -> α^0 = 1, so p0[i] = d0^d1^d2 (XOR == add in GF(256))
        std::uint8_t ref0 = u8(d0[i]) ^ u8(d1[i]) ^ u8(d2[i]);
        BOOST_TEST(u8(p0[i]) == ref0);

        // j = 1 -> α^(d) coefficients: 1, α^1, α^2
        std::uint8_t ref1 = T().mul(1, u8(d0[i]));
        ref1 ^= T().mul(T().pow_a(1), u8(d1[i]));
        ref1 ^= T().mul(T().pow_a(2), u8(d2[i]));
        BOOST_TEST(u8(p1[i]) == ref1);
    }
}

BOOST_AUTO_TEST_CASE(encode_K3_basic_shapes) {
    const auto d0 = to_bytes("aaaa");
    const auto d1 = to_bytes("bbbb");
    const auto d2 = to_bytes("cccc");
    const size_t L = d0.size();

    const std::byte* data[3] = { d0.data(), d1.data(), d2.data() };
    std::vector<std::byte> p0(L), p1(L), p2(L);
    std::byte* parity[3] = { p0.data(), p1.data(), p2.data() };

    gf256_encode(std::span<const std::byte* const>(data, 3), L,
        std::span<std::byte*>(parity, 3));

    // Linearity check: p0 = d0 ^ d1 ^ d2 (since coefficients 1,1,1)
    for (size_t i = 0; i < L; ++i) {
        const auto want0 = u8(d0[i]) ^ u8(d1[i]) ^ u8(d2[i]);
        BOOST_TEST(u8(p0[i]) == want0);
    }
}

BOOST_AUTO_TEST_SUITE_END()