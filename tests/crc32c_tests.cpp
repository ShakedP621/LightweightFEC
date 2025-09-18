#include <boost/test/unit_test.hpp>   // NOTE: not "included/unit_test.hpp"
#include <ltfec/util/crc32c.h>
#include <string>
#include <vector>

using ltfec::util::crc32c;
using ltfec::util::crc32c_init;
using ltfec::util::crc32c_update;
using ltfec::util::crc32c_finish;

BOOST_AUTO_TEST_SUITE(crc32c_suite)

BOOST_AUTO_TEST_CASE(empty_is_zero) {
    const std::string s;
    BOOST_TEST(crc32c(s) == 0u);
}

BOOST_AUTO_TEST_CASE(known_vector_123456789) {
    const std::string s = "123456789";
    // Standard CRC32C of "123456789" is 0xE3069283
    BOOST_TEST(crc32c(s) == 0xE3069283u);
}

BOOST_AUTO_TEST_CASE(incremental_matches_oneshot) {
    const std::string a = "Hello ";
    const std::string b = "world";
    const std::string all = a + b;

    uint32_t st = crc32c_init();
    st = crc32c_update(st, std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(a.data()), a.size()));
    st = crc32c_update(st, std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(b.data()), b.size()));
    const uint32_t inc = crc32c_finish(st);

    BOOST_TEST(inc == crc32c(all));
}

BOOST_AUTO_TEST_SUITE_END()