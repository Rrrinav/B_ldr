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

    suite.expect(bld::fs::make_dir_if_not_exists("") == false, "Empty path should return false");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "test") == true, "Failed to create a directory");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "test") == false, "Returned true when the directory already existed");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "test_par/test", true) == true, "Failed to create nested directories");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "test_par2/test", false) == false, "Created directory despite missing parent");
    std::ofstream(SANDBOX + "existing_file").close();
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "existing_file") == false, "Created directory where a file already exists");
    std::ofstream(SANDBOX + "file_parent").close();
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "file_parent/child", true) == false, "Created directory through a file parent");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "trailing/") == true, "Failed with trailing separator");
    std::ofstream(SANDBOX + "temp").close();
    std::filesystem::remove(SANDBOX + "temp");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "temp") == true, "Failed after file removal");
    bld::fs::make_dir_if_not_exists(SANDBOX + "idempotent");
    suite.expect(bld::fs::make_dir_if_not_exists(SANDBOX + "idempotent") == false, "Function is not idempotent");
    return suite;
}

auto test_read_write_append_remove() -> TestSuite
{
    TestSuite suite{.function = "read_write_append_remove"};

    auto file1 = SANDBOX + "rw_test1.txt";
    auto file2 = SANDBOX + "rw_test2.txt";
    auto nested = SANDBOX + "nested_dir_missing/rw_test3.txt";

    auto w_res = bld::fs::write_file(file1, "Hello");
    suite.expect(w_res.has_value(), "write_file failed");

    auto r_res = bld::fs::read_file(file1);
    suite.expect(r_res.has_value() && *r_res == "Hello", "read_file returned incorrect content");

    auto a_res = bld::fs::append_file(file1, " World");
    suite.expect(a_res.has_value(), "append_file failed");

    r_res = bld::fs::read_file(file1);
    suite.expect(r_res.has_value() && *r_res == "Hello World", "read_file returned incorrect content after append");

    a_res = bld::fs::append_file(file2, "New Append");
    suite.expect(a_res.has_value(), "append_file to new file failed");
    r_res = bld::fs::read_file(file2);
    suite.expect(r_res.has_value() && *r_res == "New Append", "append_file did not create valid new file");

    std::string bin_data = "abc\0def\0ghi";
    bin_data.resize(11);
    std::ignore = bld::fs::write_file(file1, bin_data);
    r_res = bld::fs::read_file(file1);
    suite.expect(r_res.has_value() && r_res->size() == 11 && *r_res == bin_data, "binary write/read failed");

    auto bad_w = bld::fs::write_file(nested, "fail");
    suite.expect(!bad_w.has_value(), "write_file should fail if parent directory does not exist");

    auto bad_r = bld::fs::read_file("non_existent_file_123.txt");
    suite.expect(!bad_r.has_value(), "read_file should fail on missing file");

    auto bad_dir_r = bld::fs::read_file(SANDBOX);
    suite.expect(!bad_dir_r.has_value(), "read_file should fail when target is a directory");

    auto rm_var_res = bld::fs::remove(file1, file2, "fake_file_that_does_not_exist.txt");
    suite.expect(rm_var_res.has_value(), "variadic remove failed (should succeed even if some are missing)");
    suite.expect(!bld::fs::exists(file1) && !bld::fs::exists(file2), "files still exist after variadic remove");

    return suite;
}

auto test_dir_walker() -> TestSuite
{
    TestSuite suite{.function = "dir_walker"};

    auto root = SANDBOX + "walker_test/";
    std::string dir1 = root + "dir1";
    std::string dir2 = root + "dir2";
    std::string sub_dir = dir2 + "/sub";

    auto md_res = bld::fs::make_dirs(dir1, sub_dir);
    suite.expect(md_res.has_value(), "make_dirs failed to set up test environment");

    std::ignore = bld::fs::write_file(dir1 + "/file1.txt", "a");
    std::ignore = bld::fs::write_file(dir1 + "/file2.cpp", "b");
    std::ignore = bld::fs::write_file(dir2 + "/file3.txt", "c");
    std::ignore = bld::fs::write_file(dir2 + "/.hidden_file", "d");
    std::ignore = bld::fs::write_file(sub_dir + "/file4.cpp", "e");
    std::ignore = bld::fs::write_file(root + "root_file.hpp", "f");

    // 1. Recursive collect vs Flat
    auto all_files = bld::fs::Dir_walker{root}.collect();
    suite.expect(all_files.has_value() && all_files->size() == 5, "recursive collect should find exactly 5 files (hidden excluded)");

    auto flat_files = bld::fs::Dir_walker{root}.flat().collect();
    suite.expect(flat_files.has_value() && flat_files->size() == 1, "flat collect should find exactly 1 file in root");

    // 2. Max Depth limits
    auto depth1 = bld::fs::Dir_walker{root}.max_depth(1).collect();
    suite.expect(depth1.has_value() && depth1->size() == 4, "max_depth(1) should find 4 files (root + immediate children)");

    // 3. Multiple Extensions
    auto ext_multi = bld::fs::Dir_walker{root}.ext({".cpp", ".hpp"}).collect();
    suite.expect(ext_multi.has_value() && ext_multi->size() == 3, "multi-extension filter should find 3 files");

    // 4. Custom Predicate (where)
    auto where_test = bld::fs::Dir_walker{root}.where([](const bld::fs::Dir_entry& e) {
        return e.filename().starts_with("file");
    }).collect();
    suite.expect(where_test.has_value() && where_test->size() == 4, "custom where predicate failed");

    // 5. Chained Filters
    auto chained = bld::fs::Dir_walker{root}.ext(".txt").skip("dir1").collect();
    suite.expect(chained.has_value() && chained->size() == 1, "chained filters (ext + skip) failed");

    // 6. Partition
    auto part_res = bld::fs::Dir_walker{root}.partition([](const bld::fs::Dir_entry& e) {
        return e.extension() == ".cpp";
    });
    suite.expect(part_res.has_value(), "partition failed");
    if (part_res) {
        suite.expect(part_res->first.size() == 2 && part_res->second.size() == 3, "partition split sizes incorrect");
    }

    // 7. Fold
    auto fold_res = bld::fs::Dir_walker{root}.fold(0, [](int acc, const bld::fs::Dir_entry&) {
        return acc + 1;
    });
    suite.expect(fold_res.has_value() && *fold_res == 5, "fold accumulator failed to count 5 files");

    // 8. First and Last semantics
    auto first_res = bld::fs::Dir_walker{root}.first();
    suite.expect(first_res.has_value() && first_res->has_value(), "first() should return an entry");

    auto last_res = bld::fs::Dir_walker{root}.named("file4.cpp").last();
    suite.expect(last_res.has_value() && last_res->has_value(), "last() should return the deeply nested file");

    // 9. High-level wrappers
    auto hl_ext = bld::fs::find_by_ext(root, ".cpp", ".txt");
    suite.expect(hl_ext.has_value() && hl_ext->size() == 4, "find_by_ext wrapper failed");

    auto hl_name = bld::fs::find_by_name(root, "root_file.hpp", "file3.txt");
    suite.expect(hl_name.has_value() && hl_name->size() == 2, "find_by_name wrapper failed");

    return suite;
}

int main(int argc, char *argv[])
{
    if (auto res = bld::rebuild_this_when_needed_ext(argc, argv, {"-I."}); !res) {
        bld::log::e("{}", res.error());
        return EXIT_FAILURE;
    }

    std::filesystem::remove_all(SANDBOX);
    std::filesystem::create_directories(SANDBOX);

    std::ofstream out("tests/fs/out");

    auto suite = test_make_dir_if_not_exists();
    suite.serialize(out);
    suite = test_read_write_append_remove();
    suite.serialize(out);
    suite = test_dir_walker();
    suite.serialize(out);

    std::filesystem::remove_all(SANDBOX);

    return 0;
}
