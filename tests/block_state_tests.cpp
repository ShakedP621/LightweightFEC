#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/pipeline/block_state.h>

using namespace ltfec::pipeline;

BOOST_AUTO_TEST_SUITE(block_state_suite)

BOOST_AUTO_TEST_CASE(counts_and_basic_queries) {
    BlockPolicy pol{ .N = 5, .K = 1, .reorder_ms = 50, .fps = 30 };
    BlockState st(pol);

    BOOST_TEST(st.data_seen_count() == 0u);
    BOOST_TEST(st.data_missing_count() == 5u);
    BOOST_TEST(!st.have_any_parity());

    st.mark_data(0);
    st.mark_data(2);
    st.mark_parity(0);

    BOOST_TEST(st.data_seen_count() == 2u);
    BOOST_TEST(st.data_missing_count() == 3u);
    BOOST_TEST(st.have_any_parity());
    BOOST_TEST(!st.have_all_data());
}

BOOST_AUTO_TEST_CASE(k1_recoverable_when_exactly_one_missing_and_parity_present) {
    BlockPolicy pol{ .N = 4, .K = 1, .reorder_ms = 50, .fps = 30 };
    BlockState st(pol);

    // Arrive three data frames (missing one) + parity
    st.mark_data(0);
    st.mark_data(1);
    st.mark_data(3);
    st.mark_parity(0);

    BOOST_TEST(st.data_missing_count() == 1u);
    BOOST_TEST(st.recoverable_k1());
    BOOST_TEST(st.first_missing_data() == 2);
}

BOOST_AUTO_TEST_CASE(k1_not_recoverable_when_zero_or_many_missing_or_no_parity) {
    // Zero missing
    {
        BlockPolicy pol{ .N = 3, .K = 1, .reorder_ms = 50, .fps = 30 };
        BlockState st(pol);
        st.mark_data(0); st.mark_data(1); st.mark_data(2);
        st.mark_parity(0);
        BOOST_TEST(!st.recoverable_k1());
        BOOST_TEST(st.first_missing_data() == -1);
    }
    // Two missing
    {
        BlockPolicy pol{ .N = 3, .K = 1, .reorder_ms = 50, .fps = 30 };
        BlockState st(pol);
        st.mark_data(0);
        st.mark_parity(0);
        BOOST_TEST(st.data_missing_count() == 2u);
        BOOST_TEST(!st.recoverable_k1());
        BOOST_TEST(st.first_missing_data() == -1);
    }
    // One missing but no parity
    {
        BlockPolicy pol{ .N = 4, .K = 1, .reorder_ms = 50, .fps = 30 };
        BlockState st(pol);
        st.mark_data(0); st.mark_data(1); st.mark_data(2);
        BOOST_TEST(!st.have_any_parity());
        BOOST_TEST(!st.recoverable_k1());
    }
}

BOOST_AUTO_TEST_CASE(k3_tracks_multiple_parity_lanes) {
    BlockPolicy pol{ .N = 6, .K = 3, .reorder_ms = 50, .fps = 30 };
    BlockState st(pol);

    st.mark_parity(0);
    BOOST_TEST(st.parity_seen_count() == 1u);
    st.mark_parity(2);
    BOOST_TEST(st.parity_seen_count() == 2u);
    BOOST_TEST(!st.have_all_parity());
    st.mark_parity(1);
    BOOST_TEST(st.parity_seen_count() == 3u);
    BOOST_TEST(st.have_all_parity());
}

BOOST_AUTO_TEST_SUITE_END()