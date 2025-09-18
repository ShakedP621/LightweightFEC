#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/block_tracker.h>

using namespace ltfec::pipeline;

BOOST_AUTO_TEST_SUITE(block_tracker_suite)

BOOST_AUTO_TEST_CASE(immediate_when_parity_and_all_data) {
    BlockPolicy pol;
    pol.N = 8; pol.K = 1; pol.reorder_ms = 50; pol.fps = 30;

    BlockTracker bt(pol);
    const std::uint64_t t0 = 1000;
    bt.start(t0);
    for (std::uint16_t i = 0; i < pol.N; ++i) bt.mark_data(i, t0);
    bt.mark_parity(0, t0);

    BOOST_TEST(bt.have_all_data());
    BOOST_TEST(bt.have_parity());
    BOOST_TEST(bt.should_close(t0)); // immediate close
}

BOOST_AUTO_TEST_CASE(close_at_reorder_ms_if_earlier) {
    BlockPolicy pol;
    pol.N = 8; pol.K = 1; pol.reorder_ms = 50; pol.fps = 30; // span ~ 267ms -> min(60, 533) = 60
    BlockTracker bt(pol);
    const std::uint64_t t0 = 2000;
    bt.start(t0);
    // No parity, not all data -> close by time
    BOOST_TEST(!bt.should_close(t0 + 49));
    BOOST_TEST(bt.should_close(t0 + 50)); // reorder_ms dominates since 50 < 60
}

BOOST_AUTO_TEST_CASE(close_at_min_of_60_or_2xspan_when_reorder_higher) {
    BlockPolicy pol;
    pol.N = 8; pol.K = 1; pol.reorder_ms = 200; pol.fps = 30; // min(60, 533) = 60 < 200
    BlockTracker bt(pol);
    const std::uint64_t t0 = 3000;
    bt.start(t0);
    BOOST_TEST(!bt.should_close(t0 + 59));
    BOOST_TEST(bt.should_close(t0 + 60)); // min(60, 2*span) reached first
}

BOOST_AUTO_TEST_SUITE_END()