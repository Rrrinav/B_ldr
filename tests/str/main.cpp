#include <fstream>

#define B_LDR_IMPLEMENTATION
#include "b_ldr.hpp"

const std::string SANDBOX = "tests/fs/sandbox/";

struct TestSuite
{
    std::string function;
    int total{};
    int failed{};
    std::vector<int> failed_indices{};
    std::vector<std::string> failed_messages{};
    auto expect(bool condition, std::string_view message, std::source_location loc = std::source_location::current()) -> void
    {
        if (!condition) {
            ++failed;
            failed_indices.push_back(total);
            failed_messages.emplace_back(std::format("{}:{}: {}", loc.file_name(), loc.line(), message));
        }
        ++total;
    }

    auto serialize(std::ostream &out = std::cout) const -> void
    {
        std::println(out, "FUNCTION");
        std::println(out, "{}", function);
        std::println(out, "TOTAL");
        std::println(out, "{}", total);
        std::println(out, "FAILED");
        std::println(out, "{}", failed);
        std::println(out, "FAILED_INDICES");
        for (std::size_t i = 0; i < failed_indices.size(); ++i) {
            if (i) {
                out << ',';
            }
            out << failed_indices[i];
        }
        out << '\n';
        std::println(out, "FAILED_MESSAGES");
        for (const auto &msg : failed_messages) {
            std::println(out, "{}", msg);
        }
        std::println(out, "END");
    }
};

auto test_str_utils() -> TestSuite
{
    TestSuite suite{.function = "str_utils"};

    // 1. Trimming
    suite.expect(bld::str::trim_left("   hello") == "hello", "trim_left failed on leading spaces");
    suite.expect(bld::str::trim_right("hello   ") == "hello", "trim_right failed on trailing spaces");
    suite.expect(bld::str::trim(" \t \n hello \r  ") == "hello", "trim failed on mixed whitespace");
    suite.expect(bld::str::trim("   ") == "", "trim failed on all-whitespace string");
    suite.expect(bld::str::trim("") == "", "trim failed on empty string");
    suite.expect(bld::str::trim("nospace") == "nospace", "trim modified a string with no outer spaces");

    // 2. Splitting (by char)
    auto split_c1 = bld::str::split("a,b,c", ',');
    suite.expect(split_c1.size() == 3 && split_c1[0] == "a" && split_c1[2] == "c", "Basic char split failed");

    auto split_c2 = bld::str::split("a,,c", ',');
    suite.expect(split_c2.size() == 3 && split_c2[1] == "", "Char split failed to preserve empty tokens between delimiters");

    auto split_c3 = bld::str::split("no_delim", ',');
    suite.expect(split_c3.size() == 1 && split_c3[0] == "no_delim", "Char split failed on missing delimiter");

    // 3. Splitting (by string_view)
    auto split_s1 = bld::str::split("a--b--c", "--");
    suite.expect(split_s1.size() == 3 && split_s1[1] == "b", "Basic string split failed");

    auto split_s2 = bld::str::split("a----c", "--");
    suite.expect(split_s2.size() == 3 && split_s2[1] == "", "String split failed on consecutive string delimiters");

    auto split_s3 = bld::str::split("hello", "");
    suite.expect(split_s3.size() == 1 && split_s3[0] == "hello", "String split failed to handle empty delimiter fallback");

    // 4. Case Conversion
    suite.expect(bld::str::to_lower("HeLLo 123!@#") == "hello 123!@#", "to_lower failed on mixed alphanumeric");
    suite.expect(bld::str::to_upper("hello world") == "HELLO WORLD", "to_upper failed");

    // 5. Replace All
    suite.expect(bld::str::replace_all("hello world", "world", "bld") == "hello bld", "Basic replace_all failed");
    suite.expect(bld::str::replace_all("a a a", "a", "b") == "b b b", "replace_all failed on multiple occurrences");
    suite.expect(bld::str::replace_all("foo", "bar", "baz") == "foo", "replace_all modified string when target missing");
    suite.expect(bld::str::replace_all("remove me", " ", "") == "removeme", "replace_all failed to replace with empty string");

    // 6. Parsing Integers
    suite.expect(bld::str::parse_int("42").value_or(0) == 42, "parse_int failed on basic positive");
    suite.expect(bld::str::parse_int("-10").value_or(0) == -10, "parse_int failed on basic negative");
    suite.expect(bld::str::parse_int("  15  ").value_or(0) == 15, "parse_int failed to ignore surrounding whitespace");
    suite.expect(bld::str::parse_int("1010", 2).value_or(0) == 10, "parse_int failed to parse binary base");
    suite.expect(!bld::str::parse_int("abc").has_value(), "parse_int should have failed on letters");
    suite.expect(!bld::str::parse_int("12abc").has_value(), "parse_int should have failed on mixed alphanumeric");

    // 7. Parsing Doubles
    suite.expect(bld::str::parse_double("3.14159").value_or(0.0) == 3.14159, "parse_double failed on valid decimal");
    suite.expect(bld::str::parse_double("-0.5").value_or(0.0) == -0.5, "parse_double failed on negative decimal");
    suite.expect(bld::str::parse_double("  42  ").value_or(0.0) == 42.0, "parse_double failed on whole number with spaces");
    suite.expect(!bld::str::parse_double("3.14.15").has_value(), "parse_double should have failed on malformed decimal");

    // 8. Parsing Booleans
    suite.expect(bld::str::parse_bool("true").value_or(false) == true, "parse_bool failed on 'true'");
    suite.expect(bld::str::parse_bool("TrUe").value_or(false) == true, "parse_bool failed on mixed-case 'TrUe'");
    suite.expect(bld::str::parse_bool("1").value_or(false) == true, "parse_bool failed on '1'");
    suite.expect(bld::str::parse_bool("y").value_or(false) == true, "parse_bool failed on 'y'");

    suite.expect(bld::str::parse_bool("false").value_or(true) == false, "parse_bool failed on 'false'");
    suite.expect(bld::str::parse_bool("0").value_or(true) == false, "parse_bool failed on '0'");
    suite.expect(bld::str::parse_bool(" NO ").value_or(true) == false, "parse_bool failed on padded ' NO '");

    suite.expect(!bld::str::parse_bool("maybe").has_value(), "parse_bool should have failed on invalid string");

    // 9. Joining
    std::vector<std::string> v1{"a", "b", "c"};
    suite.expect(bld::str::join(v1, "-") == "a-b-c", "join failed on standard vector");

    std::vector<std::string_view> v2{"alone"};
    suite.expect(bld::str::join(v2, ",") == "alone", "join failed to omit delimiter on single-element vector");

    std::vector<std::string> v3{};
    suite.expect(bld::str::join(v3, "|") == "", "join failed on empty vector");

    return suite;
}

int main(int argc, char *argv[])
{
    if (auto res = bld::rebuild_this_when_needed_ext(argc, argv, {"-I."}); !res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }

    std::ofstream out("tests/str/out");

    auto suite = test_str_utils();
    suite.serialize(out);
    return 0;
}
