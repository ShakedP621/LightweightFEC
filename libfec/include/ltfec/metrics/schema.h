#pragma once
#include <string>
#include <vector>

namespace ltfec::metrics {

    // Bump when columns/semantics change.
    inline constexpr int schema_version = 1;

    // Stable column order for all app CSVs:
    inline std::vector<std::string> standard_header() {
        return { "schema_version","run_uuid","ts_ms","app","event","ip","port","bytes" };
    }

} // namespace ltfec::metrics