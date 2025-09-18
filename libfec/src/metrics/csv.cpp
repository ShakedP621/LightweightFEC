#include <ltfec/metrics/csv.h>
#include <stdexcept>
#include <fstream>

namespace ltfec::metrics {

    static inline bool needs_quotes(std::string_view s) {
        for (char c : s) {
            if (c == ',' || c == '"' || c == '\n' || c == '\r') return true;
        }
        return false;
    }

    void CsvWriter::append_field_csv(std::string& out, std::string_view field) {
        if (!needs_quotes(field)) {
            out.append(field);
            return;
        }
        out.push_back('"');
        for (char c : field) {
            if (c == '"') out.push_back('"'); // escape by doubling
            out.push_back(c);
        }
        out.push_back('"');
    }

    void CsvWriter::append_row_csv(std::string& out, const std::vector<std::string>& fields) {
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i) out.push_back(',');
            append_field_csv(out, fields[i]);
        }
        out.push_back('\n');
    }

    void CsvWriter::set_header(std::vector<std::string> columns) {
        if (!header_.empty()) {
            throw std::logic_error("CsvWriter: header already set");
        }
        header_ = std::move(columns);

        // Write the header row with implicit schema fields.
        // Final header row: schema_version,run_uuid,<user header...>
        std::vector<std::string> hdr;
        hdr.reserve(2 + header_.size());
        hdr.emplace_back("schema_version");
        hdr.emplace_back("run_uuid");
        for (auto& c : header_) hdr.emplace_back(c);

        append_row_csv(buf_, hdr);
    }

    void CsvWriter::add_row(const std::vector<std::string>& fields) {
        if (header_.empty()) {
            throw std::logic_error("CsvWriter: set_header must be called before add_row");
        }
        if (fields.size() != header_.size()) {
            throw std::invalid_argument("CsvWriter: field count does not match header");
        }
        // Prefix the implicit schema fields.
        std::vector<std::string> row;
        row.reserve(2 + fields.size());
        row.emplace_back(std::to_string(schema_version_));
        row.emplace_back(run_uuid_);
        for (auto const& f : fields) row.emplace_back(f);

        append_row_csv(buf_, row);
    }

    void CsvWriter::finish_with_summary(std::string summary_text) {
        // Footer as a special commented summary row: "# summary", schema_version, run_uuid, text
        std::vector<std::string> row;
        row.reserve(4);
        row.emplace_back("# summary");
        row.emplace_back(std::to_string(schema_version_));
        row.emplace_back(run_uuid_);
        row.emplace_back(std::move(summary_text));
        append_row_csv(buf_, row);
    }

    bool CsvWriter::save_to_file(const std::string& filepath) const {
        std::ofstream os(filepath, std::ios::binary | std::ios::trunc);
        if (!os) return false;
        os.write(buf_.data(), static_cast<std::streamsize>(buf_.size()));
        return static_cast<bool>(os);
    }
} // namespace ltfec::metrics