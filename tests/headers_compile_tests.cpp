#include <boost/test/unit_test.hpp>  // NOTE: not the "included/" runner here
#include <ltfec/util/crc32c.h>
#include <ltfec/protocol/ids.h>
#include <ltfec/protocol/frame.h>
#include <ltfec/transport/udp.h>
#include <ltfec/fec_core/xor_parity.h>
#include <ltfec/fec_core/gf256.h>
#include <ltfec/pipeline/block.h>
#include <ltfec/sim/model.h>
#include <ltfec/metrics/csv.h>

BOOST_AUTO_TEST_CASE(headers_compile) {
    BOOST_TEST(true); // just ensuring headers parse & basic constants exist
}