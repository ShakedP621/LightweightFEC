#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/util/endian.h>
#include <array>
#include <cstddef>

using namespace ltfec::util::endian;

static unsigned u8(std::byte b) {
    return static_cast<unsigned>(std::to_integer<unsigned char>(b));
}

BOOST_AUTO_TEST_SUITE(endian_suite)

BOOST_AUTO_TEST_CASE(write_read_u16_le) {
    std::array<std::byte, 2> buf{};
    write_u16_le(std::span<std::byte>(buf.data(), buf.size()), 0xABCDu);
    BOOST_TEST(u8(buf[0]) == 0xCDu);
    BOOST_TEST(u8(buf[1]) == 0xABu);

    const auto v = read_u16_le(std::span<const std::byte>(buf.data(), buf.size()));
    BOOST_TEST(v == 0xABCDu);
}

BOOST_AUTO_TEST_CASE(write_read_u32_le) {
    std::array<std::byte, 4> buf{};
    write_u32_le(std::span<std::byte>(buf.data(), buf.size()), 0x12345678u);
    BOOST_TEST(u8(buf[0]) == 0x78u);
    BOOST_TEST(u8(buf[1]) == 0x56u);
    BOOST_TEST(u8(buf[2]) == 0x34u);
    BOOST_TEST(u8(buf[3]) == 0x12u);

    const auto v = read_u32_le(std::span<const std::byte>(buf.data(), buf.size()));
    BOOST_TEST(v == 0x12345678u);
}
BOOST_AUTO_TEST_SUITE_END()