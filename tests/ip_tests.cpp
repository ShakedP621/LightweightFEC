#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/transport/ip.h>

using namespace ltfec::transport;

BOOST_AUTO_TEST_SUITE(ip_suite)

BOOST_AUTO_TEST_CASE(ip_parse_and_multicast) {
    Ipv4 a{};
    BOOST_TEST(parse_ipv4("239.1.2.3", a));
    BOOST_TEST(ipv4_is_multicast(a));
    BOOST_TEST(a.oct[0] == 239u);
    BOOST_TEST(a.oct[1] == 1u);
    BOOST_TEST(a.oct[2] == 2u);
    BOOST_TEST(a.oct[3] == 3u);

    BOOST_TEST(!parse_ipv4("256.0.0.1", a));
    BOOST_TEST(!parse_ipv4("1.2.3", a));
}

BOOST_AUTO_TEST_CASE(endpoint_parse) {
    Endpoint ep{};
    BOOST_TEST(parse_endpoint("127.0.0.1:12345", ep));
    BOOST_TEST(ep.addr.oct[0] == 127u);
    BOOST_TEST(ep.addr.oct[1] == 0u);
    BOOST_TEST(ep.addr.oct[2] == 0u);
    BOOST_TEST(ep.addr.oct[3] == 1u);
    BOOST_TEST(ep.port == 12345u);

    BOOST_TEST(!parse_endpoint("1.2.3:80", ep));      // bad IPv4
    BOOST_TEST(!parse_endpoint("1.2.3.4:", ep));      // missing port
    BOOST_TEST(!parse_endpoint("1.2.3.4:70000", ep)); // port out of range
}

BOOST_AUTO_TEST_SUITE_END()