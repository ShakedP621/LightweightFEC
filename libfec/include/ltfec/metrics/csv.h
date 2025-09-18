#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

namespace ltfec::metrics {

    // Simple in-memory CSV builder.
    // Usage:
    //   CsvWriter w(1);
    //   w.set_run_uuid("123e4567-e89b-12d3-a456-426614174000");
    //   w.set_header({"ts_ms","event","value"});
    //   w.add_row({"100","frame_tx","1200"});
    //   w.finish_with_summary("ok");
    //   auto csv = w.str();
    class CsvWriter {
    public:
        explicit CsvWriter(int schema_version) : schema_version_(schema_version) {}

        void set_run_uuid(std::string uuid) { run_uuid_ = std::move(uuid); }

        // Set column names for data rows (not including the implicit schema/run_uuid prefix).
        void set_header(std::vector<std::string> columns);

        // Append one data row (same size as header()).
        void add_row(const std::vector<std::string>& fields);

        // Finalize and append a footer summary row, prefixed with "# summary".
        void finish_with_summary(std::string summary_text);

        // Get the whole CSV text.
        const std::string& str() const noexcept { return buf_; }

        // Introspection helpers (used in tests).
        const std::vector<std::string>& header() const noexcept { return header_; }
        int schema_version() const noexcept { return schema_version_; }
        const std::string& run_uuid() const noexcept { return run_uuid_; }

        // Save CSV buffer to a file path (UTF-8). Overwrites existing file.
        // Returns true on success.
        bool save_to_file(const std::string& filepath) const;

    private:
        static void append_field_csv(std::string& out, std::string_view field);
        static void append_row_csv(std::string& out, const std::vector<std::string>& fields);

        int schema_version_{ 1 };
        std::string run_uuid_;
        std::vector<std::string> header_;
        std::string buf_;
    };

} // namespace ltfec::metrics