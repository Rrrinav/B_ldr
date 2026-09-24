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

auto test_walk() -> TestSuite
{
    TestSuite suite{.function = "walk"};

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

    // The loop body handles everything; ranges compose downstream.
    auto gather = [](const std::string &rt, bld::fs::Walk_opts opts, auto pred) {
        bld::fs::Controller ctl{std::move(opts)};
        std::vector<std::string> out;
        for (const auto &e : bld::fs::walk_dir(rt, ctl)) {
            if (e.is_file() && pred(e)) {
                out.push_back(e.filename());
            }
        }
        return std::pair{!ctl.failed(), std::move(out)};
    };
    auto all_files = gather(root, {}, [](const auto &) { return true; });
    suite.expect(all_files.first && all_files.second.size() == 5, "recursive walk should find exactly 5 files (hidden excluded)");

    auto flat_files = gather(root, {.recursive = false}, [](const auto &) { return true; });
    suite.expect(flat_files.first && flat_files.second.size() == 1, "flat walk should find exactly 1 file in root");

    // 2. Max Depth limits
    auto depth1 = gather(root, {.max_depth = 1}, [](const auto &) { return true; });
    suite.expect(depth1.first && depth1.second.size() == 4, "max_depth(1) should find 4 files (root + immediate children)");

    // 3. Multiple Extensions via the adaptor (dot-insensitive)
    std::vector<std::string> ext_multi;
    {
        bld::fs::Controller ctl;
        for (const auto &e :
             bld::fs::walk_dir(root, ctl) | bld::fs::only_extensions("cpp", ".hpp")) {
            if (e.is_file()) {
                ext_multi.push_back(e.filename());
            }
        }
        suite.expect(!ctl.failed() && ext_multi.size() == 3, "multi-extension filter should find 3 files");
    }

    // 4. Custom predicate (plain views::filter, no library helper needed)
    std::vector<std::string> where_test;
    {
        bld::fs::Controller ctl;
        for (const auto &e :
             bld::fs::walk_dir(root, ctl) | std::views::filter([](const auto &x) { return x.filename().starts_with("file"); })) {
            if (e.is_file()) {
                where_test.push_back(e.filename());
            }
        }
        suite.expect(!ctl.failed() && where_test.size() == 4, "custom predicate failed");
    }

    // 5. Static skip + name exclusion adaptor
    auto chained = gather(root, {.skip = {"dir1"}}, [](const auto &e) { return e.extension() == ".txt"; });
    suite.expect(chained.first && chained.second.size() == 1, "static skip + filter failed");
    {
        bld::fs::Controller ctl;
        std::size_t n = 0;
        for (const auto &e : bld::fs::walk_dir(root, ctl) | bld::fs::not_name("file1.txt", "file2.cpp")) {
            if (e.is_file()) {
                ++n;
            }
        }
        suite.expect(!ctl.failed() && n == 3, "not_name adaptor failed");
    }

    // 6. Dynamic dont_recurse: dir2's branch is never visited nor descended.
    std::vector<std::string> pruned;
    bld::fs::Controller prune_ctl;
    for (const auto &e : bld::fs::walk_dir(root, prune_ctl)) {
        if (e.is_dir() && e.filename() == "dir2") {
            prune_ctl.dont_recurse = true;
            continue;
        }
        if (e.is_file()) {
            pruned.push_back(e.filename());
        }
    }
    suite.expect(!prune_ctl.failed() && pruned.size() == 3, "prune should leave exactly 3 files");
    suite.expect(
        std::ranges::find(pruned, "file3.txt") == pruned.end() && std::ranges::find(pruned, "file4.cpp") == pruned.end(),
        "pruned branch was still visited");

    // 7. break ends cleanly (no error recorded).
    int seen = 0;
    bld::fs::Controller stop_ctl;
    for (const auto &e : bld::fs::walk_dir(root, stop_ctl)) {
        (void)e;
        if (++seen >= 2) {
            break;
        }
    }
    suite.expect(!stop_ctl.failed() && seen == 2, "break should end the walk cleanly after 2 entries");

    // 8. abort() records a USER error (distinct from library failure).
    bld::fs::Controller fail_ctl;
    for (const auto &e : bld::fs::walk_dir(root, fail_ctl)) {
        (void)e;
        fail_ctl.abort("nope");
        break;
    }
    suite.expect(fail_ctl.failed() && fail_ctl.error().is_user_error(), "abort should record a user error");

    // 9. Missing root is a library (fs) error.
    bld::fs::Controller miss_ctl;
    for (const auto &e : bld::fs::walk_dir(SANDBOX + "nope/", miss_ctl)) {
        (void)e;
    }
    suite.expect(miss_ctl.failed() && miss_ctl.error().is_fs_error(), "missing root should record an fs error");

    // 10. files() eager + high-level wrappers
    suite.expect(bld::fs::files(root).size() == 5, "files() should find exactly 5 files");
    suite.expect(bld::fs::files(SANDBOX + "nope/").empty(), "files() on missing root should be empty");

    auto hl_ext = bld::fs::find_by_ext(root, ".cpp", ".txt");
    suite.expect(hl_ext.has_value() && hl_ext->size() == 4, "find_by_ext wrapper failed");

    auto hl_name = bld::fs::find_by_name(root, "root_file.hpp", "file3.txt");
    suite.expect(hl_name.has_value() && hl_name->size() == 2, "find_by_name wrapper failed");

    return suite;
}

auto test_walk_advanced() -> TestSuite
{
    TestSuite suite{.function = "walk_advanced"};

    auto root = SANDBOX + "adv/";
    std::ignore = bld::fs::make_dirs(root + "sub");
    std::ignore = bld::fs::write_file(root + "a.txt", "a");
    std::ignore = bld::fs::write_file(root + ".hidden", "h");
    std::ignore = bld::fs::write_file(root + "sub/b.txt", "b");

    auto count_files = [](const std::string &rt, bld::fs::Walk_opts opts, auto pred) {
        bld::fs::Controller ctl{std::move(opts)};
        std::size_t n = 0;
        for (const auto &e : bld::fs::walk_dir(rt, ctl)) {
            if (e.is_file() && pred(e)) {
                ++n;
            }
        }
        return std::pair{!ctl.failed(), n};
    };
    auto hidden = count_files(root, {.include_hidden = true}, [](const auto &) { return true; });
    suite.expect(hidden.first && hidden.second == 3, "include_hidden(true) should find 3 files");

    auto cnt = count_files(root, {}, [](const auto &) { return true; });
    suite.expect(cnt.first && cnt.second == 2, "count should be 2 without hidden");

    // any/none via early break.
    bool any = false;
    {
        bld::fs::Controller ctl;
        for (const auto &e : bld::fs::walk_dir(root, ctl)) {
            if (e.is_file()) {
                any = true;
                break;
            }
        }
        suite.expect(!ctl.failed(), "any walk should not fail");
    }
    suite.expect(any, "any should be true");

    // first match via break; last match by remembering.
    std::optional<bld::fs::Dir_entry> first;
    {
        bld::fs::Controller ctl;
        for (const auto &e : bld::fs::walk_dir(root, ctl)) {
            if (e.is_file()) {
                first = e;
                break;
            }
        }
    }
    suite.expect(first.has_value(), "first should return an entry");
    std::optional<bld::fs::Dir_entry> last;
    {
        bld::fs::Controller ctl;
        for (const auto &e : bld::fs::walk_dir(root, ctl)) {
            if (e.is_file() && e.filename() == "b.txt") {
                last = e;
            }
        }
        suite.expect(!ctl.failed(), "last walk should not fail");
    }
    suite.expect(last.has_value(), "last should return the deeply nested file");

    // subdir: just walk the joined path.
    suite.expect(bld::fs::files(root + "sub").size() == 1, "subdir walk should find 1 file");

    // Regression: extensions without a leading dot must match too.
    auto dot_count = [&](std::string_view ext) {
        std::string want{ext};
        if (!want.empty() && want.front() != '.') {
            want = '.' + want;
        }
        return count_files(root, {}, [&](const auto &e) { return e.extension() == want; }).second;
    };
    suite.expect(dot_count("txt") == 2 && dot_count(".txt") == 2, "ext without dot should match ext with dot");

    // Adaptor over a subdirectory.
    {
        bld::fs::Controller ctl;
        std::size_t n = 0;
        for (const auto &e : bld::fs::walk_dir(root, ctl) | bld::fs::only_extensions({"txt"})) {
            if (e.is_file()) {
                ++n;
            }
        }
        suite.expect(!ctl.failed() && n == 2, "only_extensions with vector should match");
    }

    auto all = bld::fs::find_all_files(root);
    suite.expect(all.has_value() && all->size() == 2, "find_all_files should find 2 files");
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
    suite = test_walk();
    suite.serialize(out);
    suite = test_copy_rename_links();
    suite.serialize(out);
    suite = test_file_queries();
    suite.serialize(out);
    suite = test_walk_advanced();
    suite.serialize(out);

    std::filesystem::remove_all(SANDBOX);

    return 0;
}
