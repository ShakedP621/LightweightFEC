#include <boost/test/unit_test.hpp>  // not the included runner
#include <ltfec/metrics/csv.h>
#include <string>

using ltfec::metrics::CsvWriter;

BOOST_AUTO_TEST_SUITE(metrics_csv_suite)

BOOST_AUTO_TEST_CASE(basic_rows_and_footer) {
    CsvWriter w(3);
    w.set_run_uuid("run-123");
    w.set_header({ "col1","col2" });

    w.add_row({ "a","b" });
    w.add_row({ "1","2" });
    w.finish_with_summary("ok");

    const std::string csv = w.str();

    // Header line
    BOOST_TEST(csv.find("schema_version,run_uuid,col1,col2\n") == 0u);

    // Data lines with schema prefix
    BOOST_TEST(csv.find("3,run-123,a,b\n") != std::string::npos);
    BOOST_TEST(csv.find("3,run-123,1,2\n") != std::string::npos);

    // Footer
    BOOST_TEST(csv.find("# summary,3,run-123,ok\n") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(csv_escaping) {
    CsvWriter w(1);
    w.set_run_uuid("u\"id"); // contains a quote
    w.set_header({ "name","note" });

    w.add_row({ "Doe, John","he said: \"hi\"\nnext" });
    w.finish_with_summary("sum, mary");

    const std::string csv = w.str();

    // Header
    BOOST_TEST(csv.find("schema_version,run_uuid,name,note\n") != std::string::npos);

    // Quoted fields: commas, quotes, newlines
    // The run_uuid has a quote, so it must be quoted and internal quotes doubled.
    BOOST_TEST(csv.find("1,\"u\"\"id\",\"Doe, John\",\"he said: \"\"hi\"\"") != std::string::npos);

    // Footer should also be quoted where needed
    BOOST_TEST(csv.find("# summary,1,\"u\"\"id\",\"sum, mary\"\n") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()