#pragma once
#include <string_view>

namespace ltfec {

    // Semantic version for this release.
    inline constexpr int k_version_major = 0;
    inline constexpr int k_version_minor = 1;
    inline constexpr int k_version_patch = 0;

    inline constexpr std::string_view k_version_str = "0.1.0";

    // Convenience accessor used by apps/tests.
    inline const char* version() noexcept {
        return k_version_str.data();
    }

} // namespace ltfec