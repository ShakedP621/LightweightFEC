#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/sim/rng.h>
#include <ltfec/sim/loss.h>

using namespace ltfec::sim;

BOOST_AUTO_TEST_SUITE(sim_suite)

BOOST_AUTO_TEST_CASE(bernoulli_edges) {
    XorShift32 rng(123u);

    BernoulliLoss b0; b0.p_loss = 0.0;
    for (int i = 0; i < 100; ++i) {
        BOOST_TEST(!b0.drop(rng)); // never drops at p=0
    }

    BernoulliLoss b1; b1.p_loss = 1.0;
    for (int i = 0; i < 100; ++i) {
        BOOST_TEST(b1.drop(rng)); // always drops at p=1
    }
}

BOOST_AUTO_TEST_CASE(gilbert_toggle_alternating) {
    // Force deterministic alternation: always transition and always drop in Bad.
    XorShift32 rng(42u);
    GilbertElliottLoss ge;
    ge.p_g_to_b = 1.0;
    ge.p_b_to_g = 1.0;
    ge.p_loss_bad = 1.0;

    // Starting in Good -> transition to Bad -> drop = true
    // Next step: Bad -> transition to Good -> drop = false
    // Repeat: T, F, T, F, ...
    for (int i = 0; i < 6; ++i) {
        bool d = ge.drop(rng);
        BOOST_TEST(d == (i % 2 == 0));
    }
}

BOOST_AUTO_TEST_CASE(jitter_bounds) {
    XorShift32 rng(777u);
    BOOST_TEST(jitter_uniform_ms(rng, 0) == 0u);

    for (int i = 0; i < 100; ++i) {
        auto j = jitter_uniform_ms(rng, 50);
        BOOST_TEST(j <= 50u);
    }
}

BOOST_AUTO_TEST_SUITE_END()