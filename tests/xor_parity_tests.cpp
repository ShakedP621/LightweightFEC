#include <boost/test/unit_test.hpp>  // NOTE: not the included runner here
#include <ltfec/fec_core/xor_parity.h>
#include <vector>
#include <string>
#include <cstddef>

using ltfec::fec_core::xor_parity_1;

static std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
    return v;
}

static unsigned u8(std::byte b) {
    return static_cast<unsigned>(std::to_integer<unsigned char>(b));
}

BOOST_AUTO_TEST_SUITE(xor_parity_suite)

BOOST_AUTO_TEST_CASE(xor_parity_basic) {
    const auto f0 = to_bytes("Hello ");
    const auto f1 = to_bytes("world!");
    const auto f2 = to_bytes("ABCDE!");

    // All frames same length for this test.
    const size_t L = f0.size();
    BOOST_REQUIRE_EQUAL(f1.size(), L);
    BOOST_REQUIRE_EQUAL(f2.size(), L);

    std::vector<std::byte> parity(L);

    const std::byte* ptrs[3] = { f0.data(), f1.data(), f2.data() };
    xor_parity_1(std::span<const std::byte* const>(ptrs, 3), L, std::span<std::byte>(parity));

    // Verify bytewise parity
    for (size_t i = 0; i < L; ++i) {
        const auto x = f0[i] ^ f1[i] ^ f2[i];
        BOOST_TEST(u8(parity[i]) == u8(x));
    }
}

BOOST_AUTO_TEST_CASE(xor_parity_recover_missing_one) {
    const auto f0 = to_bytes("seven__");
    const auto f1 = to_bytes("letter_");
    const auto f2 = to_bytes("frames!");

    const size_t L = f0.size();
    BOOST_REQUIRE_EQUAL(f1.size(), L);
    BOOST_REQUIRE_EQUAL(f2.size(), L);

    std::vector<std::byte> parity(L);
    const std::byte* all[3] = { f0.data(), f1.data(), f2.data() };
    xor_parity_1(std::span<const std::byte* const>(all, 3), L, std::span<std::byte>(parity));

    // Recover f1 by XORing parity with remaining frames (f0, f2)
    std::vector<std::byte> recovered(L);
    std::copy(parity.begin(), parity.end(), recovered.begin());
    for (size_t i = 0; i < L; ++i) {
        recovered[i] ^= f0[i];
        recovered[i] ^= f2[i];
    }

    for (size_t i = 0; i < L; ++i) {
        BOOST_TEST(u8(recovered[i]) == u8(f1[i]));
    }
}

BOOST_AUTO_TEST_CASE(xor_parity_zero_frames_yields_zero) {
    const size_t L = 8;
    std::vector<std::byte> parity(L);
    const std::byte* none = nullptr;
    // No frames: N=0
    xor_parity_1(std::span<const std::byte* const>(&none, 0), L, std::span<std::byte>(parity));
    for (auto b : parity) {
        BOOST_TEST(u8(b) == 0u);
    }
}
BOOST_AUTO_TEST_SUITE_END()