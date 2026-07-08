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
auto test_make_dir_if_not_exists() -> TestSuite
{
    TestSuite suite{.function = "make_dir_if_not_exists"};

    suite.expect(bld::make_dir_if_not_exists("") == false, "Empty path should return false");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "test") == true, "Failed to create a directory");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "test") == false, "Returned true when the directory already existed");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "test_par/test", true) == true, "Failed to create nested directories");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "test_par2/test", false) == false, "Created directory despite missing parent");
    std::ofstream(SANDBOX + "existing_file").close();
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "existing_file") == false, "Created directory where a file already exists");
    std::ofstream(SANDBOX + "file_parent").close();
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "file_parent/child", true) == false, "Created directory through a file parent");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "trailing/") == true, "Failed with trailing separator");
    std::ofstream(SANDBOX + "temp").close();
    std::filesystem::remove(SANDBOX + "temp");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "temp") == true, "Failed after file removal");
    bld::make_dir_if_not_exists(SANDBOX + "idempotent");
    suite.expect(bld::make_dir_if_not_exists(SANDBOX + "idempotent") == false, "Function is not idempotent");
    return suite;
}

int main(int argc, char *argv[])
{
    bld::rebuild_this_when_needed_ext(argc, argv, {"-I."});

    std::filesystem::remove_all(SANDBOX);
    std::filesystem::create_directories(SANDBOX);

    std::ofstream out("tests/fs/out");

    auto suite = test_make_dir_if_not_exists();
    suite.serialize(out);

    std::filesystem::remove_all(SANDBOX);

    return 0;
}
