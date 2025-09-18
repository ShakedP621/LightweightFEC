#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/fec_encoder.h>
#include <ltfec/protocol/ids.h>
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

using namespace ltfec::pipeline;

static std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
    return v;
}
static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

BOOST_AUTO_TEST_SUITE(fec_encoder_suite)

BOOST_AUTO_TEST_CASE(xor_k1_parity) {
    const auto d0 = to_bytes("AAAA");
    const auto d1 = to_bytes("BBBB");
    const auto d2 = to_bytes("CCCC");
    const size_t L = d0.size();

    const std::byte* data[3] = { d0.data(), d1.data(), d2.data() };
    std::vector<std::byte> p(L);
    std::byte* parity[1] = { p.data() };

    FecEncoder enc({ .N = 3, .K = 1, .fec_scheme_id = 0 });
    const auto scheme = enc.encode(std::span<const std::byte* const>(data, 3), L,
        std::span<std::byte*>(parity, 1));
    BOOST_TEST(scheme == static_cast<std::uint8_t>(ltfec::protocol::fec_scheme_id::xor_k1));
    for (size_t i = 0; i < L; ++i) {
        const auto want = u8(d0[i]) ^ u8(d1[i]) ^ u8(d2[i]);
        BOOST_TEST(u8(p[i]) == want);
    }
}

BOOST_AUTO_TEST_CASE(gf256_k2_basic) {
    const auto d0 = to_bytes("wxyz12");
    const auto d1 = to_bytes("MNOPQR");
    const auto d2 = to_bytes("abcDEF");
    const size_t L = d0.size();

    const std::byte* data[3] = { d0.data(), d1.data(), d2.data() };
    std::vector<std::byte> p0(L), p1(L);
    std::byte* parity[2] = { p0.data(), p1.data() };

    FecEncoder enc({ .N = 3, .K = 2, .fec_scheme_id = 0 });
    const auto scheme = enc.encode(std::span<const std::byte* const>(data, 3), L,
        std::span<std::byte*>(parity, 2));
    BOOST_TEST(scheme == static_cast<std::uint8_t>(ltfec::protocol::fec_scheme_id::gf256_k2));

    // Sanity: p0 must equal XOR of data frames (coefficients 1,1,1)
    for (size_t i = 0; i < L; ++i) {
        const auto want0 = u8(d0[i]) ^ u8(d1[i]) ^ u8(d2[i]);
        BOOST_TEST(u8(p0[i]) == want0);
    }
}

BOOST_AUTO_TEST_SUITE_END()