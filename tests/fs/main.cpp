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
    suite.expect(w_res, "write_file failed");

    auto r_res = bld::fs::read_file(file1);
    suite.expect(r_res.has_value() && *r_res == "Hello", "read_file returned incorrect content");

    auto a_res = bld::fs::append_file(file1, " World");
    suite.expect(a_res, "append_file failed");

    r_res = bld::fs::read_file(file1);
    suite.expect(r_res.has_value() && *r_res == "Hello World", "read_file returned incorrect content after append");

    a_res = bld::fs::append_file(file2, "New Append");
    suite.expect(a_res, "append_file to new file failed");
    r_res = bld::fs::read_file(file2);
    suite.expect(r_res.has_value() && *r_res == "New Append", "append_file did not create valid new file");

    std::string bin_data = "abc\0def\0ghi";
    bin_data.resize(11);
    std::ignore = bld::fs::write_file(file1, bin_data);
    r_res = bld::fs::read_file(file1);
    suite.expect(r_res.has_value() && r_res->size() == 11 && *r_res == bin_data, "binary write/read failed");

    auto bad_w = bld::fs::write_file(nested, "fail");
    suite.expect(!bad_w, "write_file should fail if parent directory does not exist");

    auto bad_r = bld::fs::read_file("non_existent_file_123.txt");
    suite.expect(!bad_r.has_value(), "read_file should fail on missing file");

    auto bad_dir_r = bld::fs::read_file(SANDBOX);
    suite.expect(!bad_dir_r.has_value(), "read_file should fail when target is a directory");

    auto rm_var_res = bld::fs::remove(file1, file2, "fake_file_that_does_not_exist.txt");
    suite.expect(rm_var_res, "variadic remove failed (should succeed even if some are missing)");
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
    suite.expect(md_res, "make_dirs failed to set up test environment");

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

auto test_copy_rename_links() -> TestSuite
{
    TestSuite suite{.function = "copy_rename_links"};

    auto src = SANDBOX + "link_src.txt";
    auto copy = SANDBOX + "link_copy.txt";
    auto moved = SANDBOX + "link_moved.txt";
    std::ignore = bld::fs::write_file(src, "payload");

    suite.expect(bld::fs::copy_file(src, copy), "copy_file failed");
    auto back = bld::fs::read_file(copy);
    suite.expect(back.has_value() && *back == "payload", "copied content mismatch");
    suite.expect(!bld::fs::copy_file(src, copy), "copy without overwrite should fail when dest exists");
    suite.expect(bld::fs::copy_file(src, copy, true), "copy with overwrite failed");

    suite.expect(bld::fs::rename(copy, moved), "rename failed");
    suite.expect(!bld::fs::exists(copy) && bld::fs::exists(moved), "rename did not move the file");

    suite.expect(bld::fs::same_file(src, src), "same path should be same file");
    suite.expect(bld::fs::same_file(src, "./" + src), "relative spelling should be same file");
    suite.expect(!bld::fs::same_file(src, moved), "different files reported as same");
    suite.expect(!bld::fs::same_file(src, SANDBOX + "missing.txt"), "missing file should not be same");
    suite.expect(!bld::fs::same_file("", ""), "empty paths should not be same");

#ifndef _WIN32
    auto link = SANDBOX + "link_sym";
    auto hard = SANDBOX + "link_hard";
    // NOTE: symlink targets resolve relative to the link's own directory.
    suite.expect(bld::fs::create_symlink("link_src.txt", link), "create_symlink failed");
    suite.expect(bld::fs::is_symlink(link), "is_symlink missed the link");
    suite.expect(!bld::fs::is_symlink(src), "is_symlink false positive on regular file");
    auto target = bld::fs::read_symlink(link);
    suite.expect(target.has_value() && target->find("link_src.txt") != std::string::npos, "read_symlink target wrong");
    suite.expect(bld::fs::same_file(link, src), "symlink should be same file as target");
    suite.expect(bld::fs::create_hard_link(src, hard), "create_hard_link failed");
    suite.expect(bld::fs::same_file(hard, src), "hard link should be same file as target");
#endif
    return suite;
}

auto test_file_queries() -> TestSuite
{
    TestSuite suite{.function = "file_queries"};

    auto sized = SANDBOX + "sized.txt";
    auto empty = SANDBOX + "empty.txt";
    std::ignore = bld::fs::write_file(sized, "12345");
    std::ignore = bld::fs::write_file(empty, "");

    auto sz = bld::fs::file_size(sized);
    suite.expect(sz.has_value() && *sz == 5, "file_size wrong");
    suite.expect(!bld::fs::file_size(SANDBOX + "missing.txt").has_value(), "file_size should fail on missing file");

    auto e1 = bld::fs::is_empty(empty);
    auto e2 = bld::fs::is_empty(sized);
    suite.expect(e1.has_value() && *e1, "empty file not reported empty");
    suite.expect(e2.has_value() && !*e2, "non-empty file reported empty");

    suite.expect(bld::fs::last_write_time(sized).has_value(), "last_write_time failed");
    suite.expect(!bld::fs::last_write_time(SANDBOX + "missing.txt").has_value(), "last_write_time should fail on missing");

    auto cwd = bld::fs::current_path();
    suite.expect(cwd.has_value() && !cwd->empty(), "current_path failed");
    auto abs = bld::fs::absolute(sized);
    suite.expect(abs.has_value() && abs->ends_with(sized), "absolute wrong");
    suite.expect(bld::fs::canonical(".").has_value(), "canonical('.') failed");
    suite.expect(!bld::fs::canonical(SANDBOX + "missing.txt").has_value(), "canonical should fail on missing");
    auto rel = bld::fs::relative("/a/b/c", "/a/b");
    suite.expect(rel.has_value() && *rel == "c", "relative('/a/b/c','/a/b') should be 'c'");

    // Pure path utilities (no filesystem access).
    suite.expect(bld::fs::stem("src/main.cpp") == "main", "stem wrong");
    suite.expect(bld::fs::name("src/main.cpp") == "main.cpp", "name wrong");
    suite.expect(bld::fs::extension("src/main.cpp") == ".cpp", "extension wrong");
    suite.expect(bld::fs::parent_dir("src/main.cpp") == "src", "parent_dir wrong");
    suite.expect(bld::fs::is_absolute("/a/b"), "is_absolute missed absolute path");
    suite.expect(bld::fs::is_relative("a/b"), "is_relative missed relative path");
    suite.expect(bld::fs::join("a", "b", "c.o") == "a/b/c.o", "join wrong");
    suite.expect(bld::fs::is_dir(SANDBOX), "is_dir missed sandbox");
    suite.expect(bld::fs::is_file(sized), "is_file missed regular file");
    suite.expect(!bld::fs::is_file(SANDBOX), "is_file false positive on directory");
    return suite;
}

auto test_walker_advanced() -> TestSuite
{
    TestSuite suite{.function = "walker_advanced"};

    auto root = SANDBOX + "adv/";
    std::ignore = bld::fs::make_dirs(root + "sub");
    std::ignore = bld::fs::write_file(root + "a.txt", "a");
    std::ignore = bld::fs::write_file(root + ".hidden", "h");
    std::ignore = bld::fs::write_file(root + "sub/b.txt", "b");

    auto hidden = bld::fs::Dir_walker{root}.include_hidden(true).collect();
    suite.expect(hidden.has_value() && hidden->size() == 3, "include_hidden(true) should find 3 files");

    auto cnt = bld::fs::Dir_walker{root}.count();
    suite.expect(cnt.has_value() && *cnt == 2, "count should be 2 without hidden");

    auto any = bld::fs::Dir_walker{root}.any();
    auto none = bld::fs::Dir_walker{root}.none();
    suite.expect(any.has_value() && *any, "any should be true");
    suite.expect(none.has_value() && !*none, "none should be false");
    auto none_empty = bld::fs::Dir_walker{root}.where([](const bld::fs::Dir_entry &) { return false; }).none();
    suite.expect(none_empty.has_value() && *none_empty, "none should be true when nothing matches");

    auto paths = bld::fs::Dir_walker{root}.collect_paths();
    suite.expect(paths.has_value() && paths->size() == 2, "collect_paths should find 2 paths");

    auto sub = bld::fs::Dir_walker{root}.subdir("sub").collect();
    suite.expect(sub.has_value() && sub->size() == 1, "subdir should find 1 file");

    // Regression: extensions without a leading dot must match too.
    auto no_dot = bld::fs::Dir_walker{root}.ext({"txt"}).collect();
    auto with_dot = bld::fs::Dir_walker{root}.ext({".txt"}).collect();
    suite.expect(no_dot.has_value() && with_dot.has_value() && no_dot->size() == with_dot->size() && no_dot->size() == 2,
                 "ext without dot should match ext with dot");

    auto missing = bld::fs::Dir_walker{SANDBOX + "nope/"}.collect();
    suite.expect(!missing.has_value(), "walker on missing root should fail");

    auto all = bld::fs::find_all_files(root);
    suite.expect(all.has_value() && all->size() == 2, "find_all_files should find 2 files");
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
    suite = test_copy_rename_links();
    suite.serialize(out);
    suite = test_file_queries();
    suite.serialize(out);
    suite = test_walker_advanced();
    suite.serialize(out);

    std::filesystem::remove_all(SANDBOX);

    return 0;
}
