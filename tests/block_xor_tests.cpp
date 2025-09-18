#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/fec_core/block_xor.h>
#include <vector>
#include <string>
#include <cstddef>

using namespace ltfec::fec_core;

static std::vector<std::byte> to_bytes(const std::string& s) {
    std::vector<std::byte> v(s.size());
    for (size_t i = 0; i < s.size(); ++i) v[i] = std::byte{ static_cast<unsigned char>(s[i]) };
    return v;
}
static unsigned u8(std::byte b) { return static_cast<unsigned>(std::to_integer<unsigned char>(b)); }

BOOST_AUTO_TEST_SUITE(block_xor_suite)

BOOST_AUTO_TEST_CASE(parity_and_recover_one) {
    // Three equal-length frames.
    const auto f0 = to_bytes("alpha__");
    const auto f1 = to_bytes("beta___");
    const auto f2 = to_bytes("gamma__");
    const size_t L = f0.size();
    BOOST_REQUIRE_EQUAL(f1.size(), L);
    BOOST_REQUIRE_EQUAL(f2.size(), L);

    // Compute parity.
    std::vector<std::byte> parity(L);
    const std::byte* ptrs[3] = { f0.data(), f1.data(), f2.data() };
    block_xor_parity(std::span<const std::byte* const>(ptrs, 3), L, std::span<std::byte>(parity));

    // Simulate dropping f1: pass nullptr at index 1.
    const std::byte* with_gap[3] = { f0.data(), nullptr, f2.data() };
    std::vector<std::byte> rec(L);
    const int idx = block_xor_recover_one(std::span<const std::byte* const>(with_gap, 3), L,
        std::span<const std::byte>(parity), std::span<std::byte>(rec));
    BOOST_TEST(idx == 1);

    for (size_t i = 0; i < L; ++i) {
        BOOST_TEST(u8(rec[i]) == u8(f1[i]));
    }
}

BOOST_AUTO_TEST_CASE(fail_when_zero_or_many_missing) {
    const auto f0 = to_bytes("abcdef");
    const auto f1 = to_bytes("ghijkl");
    const size_t L = f0.size();
    std::vector<std::byte> parity(L);
    const std::byte* all[2] = { f0.data(), f1.data() };
    block_xor_parity(std::span<const std::byte* const>(all, 2), L, std::span<std::byte>(parity));

    // No missing
    std::vector<std::byte> out(L);
    const int none = block_xor_recover_one(std::span<const std::byte* const>(all, 2), L,
        std::span<const std::byte>(parity), std::span<std::byte>(out));
    BOOST_TEST(none == -1);

    // Two missing
    const std::byte* two_missing[2] = { nullptr, nullptr };
    const int many = block_xor_recover_one(std::span<const std::byte* const>(two_missing, 2), L,
        std::span<const std::byte>(parity), std::span<std::byte>(out));
    BOOST_TEST(many == -1);
}

BOOST_AUTO_TEST_SUITE_END()