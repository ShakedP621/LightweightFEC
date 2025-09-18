#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace ltfec::metrics {

    // Minimal shape for the CSV writer; impl later.
    class CsvWriter {
    public:
        explicit CsvWriter(int schema_version) : schema_version_(schema_version) {}
        void set_run_uuid(std::string uuid) { run_uuid_ = std::move(uuid); }

        // Placeholder API; actual I/O later.
        void set_header(std::vector<std::string> columns) { columns_ = std::move(columns); }

    private:
        int schema_version_{ 1 };
        std::string run_uuid_;
        std::vector<std::string> columns_;
    };

} // namespace ltfec::metrics