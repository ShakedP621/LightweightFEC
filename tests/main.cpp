// Use Boost.Test header-only runner to avoid linking the unit_test_framework lib.
#define BOOST_TEST_MODULE LightweightFEC_Tests
#include <boost/test/included/unit_test.hpp>

#include <ltfec/version.h>
#include <cstdlib>
#include <memory>

// Returns true if the env var exists and has a non-empty value.
static bool env_present(const char* name) {
    char* raw = nullptr;
    size_t len = 0;
    // _dupenv_s allocates a copy when the variable exists; we must free() it.
    if (_dupenv_s(&raw, &len, name) != 0) {
        return false;
    }
    std::unique_ptr<char, decltype(&std::free)> hold(raw, &std::free);
    return raw != nullptr && len > 1 && raw[0] != '\0'; // len includes the null terminator
}

BOOST_AUTO_TEST_CASE(version_smoke) {
    BOOST_TEST(ltfec::version() == std::string_view{ "0.1.0" });
}

// CI default is "quick" mode. We'll use FULL mode later for 120s simulations.
// Trigger FULL with environment variable LTFEC_FULL_TESTS=1.
BOOST_AUTO_TEST_CASE(mode_default_is_quick) {
    const bool full = env_present("LTFEC_FULL_TESTS");
    BOOST_TEST(!full); // default quick
}
